#pragma once

#include <atomic>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "core/job_system/inplace_job_function.h"
#include "core/job_system/intrusive_free_list.h"
#include "core/job_system/work_stealing_deque.h"
#include "mizu_core_module.h"

namespace Mizu
{

constexpr size_t MaxJobDependencies = 8;

enum class JobAffinity
{
    Any,
    Main,
    SimulationHigh,
    Simulation,
    RenderHigh,
    Render,
};

class JobSystem2;

struct JobHandle2
{
    IntrusiveFreeListIndex completion_index = IntrusiveFreeListInvalidIndex;
    size_t generation = 0;
    JobSystem2* owner = nullptr;

    bool is_valid() const { return completion_index != IntrusiveFreeListInvalidIndex; }
};

class JobDescription
{
  public:
    template <typename Func, typename... Args>
    static JobDescription create(Func&& func, Args&&... args)
    {
        JobDescription desc{};
        desc.m_func = InplaceJobFunction::create(func, args...);

        return desc;
    }

    JobDescription&& name(std::string_view new_name) &&
    {
        m_name = new_name;
        return std::move(*this);
    }

    JobDescription&& affinity(JobAffinity affinity) &&
    {
        m_affinity = affinity;
        return std::move(*this);
    }

  private:
    InplaceJobFunction m_func{};
    std::string_view m_name{};
    JobAffinity m_affinity = JobAffinity::Any;

    friend class PendingJob;
    friend class JobSystem2;
};

class PendingJob
{
  public:
    PendingJob(const PendingJob&) = delete;
    PendingJob& operator=(const PendingJob&) = delete;
    PendingJob(PendingJob&&) = delete;
    PendingJob& operator=(PendingJob&&) = delete;

    ~PendingJob()
    {
        MIZU_ASSERT(m_submitted, "The Job was not submitted, did you forget to call `schedule().submit()`?");
    }

    PendingJob& name(std::string_view name)
    {
        m_desc.m_name = name;
        return *this;
    }

    PendingJob& affinity(JobAffinity affinity)
    {
        m_desc.m_affinity = affinity;
        return *this;
    }

    PendingJob& depends_on(JobHandle2 handle)
    {
        m_dependencies.push_back(handle);
        return *this;
    }

    JobHandle2 submit();

  private:
    JobSystem2* m_owner = nullptr;

    JobDescription m_desc{};
    inplace_vector<JobHandle2, MaxJobDependencies> m_dependencies{};

    bool m_submitted = false;

    template <typename Func, typename... Args>
    PendingJob(JobSystem2* owner, Func&& func, Args&&... args) : m_owner(owner)
    {
        m_desc = JobDescription{}.create(func, args...);
    }

    friend class JobSystem2;
};

class PendingBatch
{
  public:
    PendingBatch(const PendingBatch&) = delete;
    PendingBatch& operator=(const PendingBatch&) = delete;
    PendingBatch(PendingBatch&&) = delete;
    PendingBatch& operator=(PendingBatch&&) = delete;

    ~PendingBatch()
    {
        MIZU_ASSERT(m_submitted, "The Batch was not submitted, did you forget to call `schedule_batch().submit()`?");
    }

    PendingBatch& add(JobDescription&& desc)
    {
        m_jobs.push_back(std::move(desc));
        return *this;
    }

    template <typename Func, typename... Args>
    PendingBatch& add(Func&& func, Args&&... args)
    {
        return add(JobDescription::create(func, args...));
    }

    PendingBatch& depends_on(JobHandle2 handle)
    {
        m_dependencies.push_back(handle);
        return *this;
    }

    JobHandle2 submit();

  private:
    JobSystem2* m_owner = nullptr;

    static constexpr size_t MaxBatchJobs = 64;
    inplace_vector<JobDescription, MaxBatchJobs> m_jobs{};
    inplace_vector<JobHandle2, MaxJobDependencies> m_dependencies{};

    bool m_submitted = false;

    PendingBatch(JobSystem2* owner) : m_owner(owner) {}

    friend class JobSystem2;
};

enum class JobState
{
    Free,
    PendingDependencies,
    Ready,
    Running,
    Waiting,
    Finished,
};

enum class WaitNodeType
{
    DependencyEdge,
    SuspendedTask,
    CounterWait,
};

struct IntrusiveFreeListRecordBase
{
    IntrusiveFreeListIndex pool_index = IntrusiveFreeListInvalidIndex;
    IntrusiveFreeListIndex next_free = IntrusiveFreeListInvalidIndex;
};

struct JobRecord : public IntrusiveFreeListRecordBase
{
    JobState state = JobState::Free;
    InplaceJobFunction func{};

    uint32_t num_remaining_dependencies = 0;
    IntrusiveFreeListIndex completion_index = IntrusiveFreeListInvalidIndex;

    // For suspension during execution (wait_for)
    IntrusiveFreeListIndex waiting_on_completion_index = IntrusiveFreeListInvalidIndex;
    IntrusiveFreeListIndex waiting_on_counter_index = IntrusiveFreeListInvalidIndex;
};

struct CompletionRecord : public IntrusiveFreeListRecordBase
{
    std::atomic<bool> is_completed = false;
    // Number of jobs that need to finish before we consider this CompletionRecord completed.
    std::atomic<uint32_t> counter = 0;

    // -1 to differentiate with IntrusiveFreeListInvalidIndex. IntrusiveFreeListInvalidIndex means the list is empty.
    static constexpr IntrusiveFreeListIndex ClosedWaitingList = std::numeric_limits<IntrusiveFreeListIndex>::max() - 1;
    std::atomic<IntrusiveFreeListIndex> waiting_jobs_head = IntrusiveFreeListInvalidIndex;

    std::atomic<uint32_t> references = 0;
    uint32_t generation = 0;
};

struct WaitNode : public IntrusiveFreeListRecordBase
{
    WaitNodeType type{};
    IntrusiveFreeListIndex target_job_index = IntrusiveFreeListInvalidIndex;
    IntrusiveFreeListIndex next = IntrusiveFreeListInvalidIndex;

    // For resuming jobs, prefer the same worker it was already being executed in for cache locality purposes.
    // It's not a guarantee, just a hint.
    uint32_t preferred_resume_worker_id = 0;
};

class MIZU_CORE_API JobSystem2
{
  public:
    bool init(uint32_t num_workers);

    template <typename Func, typename... Args>
    PendingJob schedule(Func&& func, Args&&... args)
    {
        return PendingJob{this, std::forward<Func>(func), std::forward<Args>(args)...};
    }

    PendingBatch schedule_batch() { return PendingBatch{this}; }

  private:
    static constexpr size_t WorkerQueueCapacity = 128;
    static constexpr size_t PoolCapacity = 2048;

    struct WorkerInfo
    {
        WorkStealingDeque<JobRecord, WorkerQueueCapacity> local_queue;
    };

    IntrusiveFreeList<JobRecord, PoolCapacity> m_job_record_pool;
    IntrusiveFreeList<CompletionRecord, PoolCapacity> m_completion_record_pool;
    IntrusiveFreeList<WaitNode, PoolCapacity> m_wait_node_pool;

    void worker_job();

    JobHandle2 submit_internal(PendingJob&& job);
    JobHandle2 submit_internal(PendingBatch&& batch);

    void init_job_record(JobDescription& job, JobRecord& job_record, const CompletionRecord& completion_record);
    void init_completion_record(CompletionRecord& completion_record, uint32_t counter);

    CompletionRecord* try_get_job_handle_completion_record(const JobHandle2& handle);
    CompletionRecord& get_job_handle_completion_record(const JobHandle2& handle);
    bool try_push_wait_node(CompletionRecord& completion_record, WaitNode& wait_node);

    JobRecord& allocate_job_record();
    CompletionRecord& allocate_completion_record();
    WaitNode& allocate_wait_node();

    void free_wait_node(const WaitNode& wait_node);

    friend class PendingJob;
    friend class PendingBatch;
};

} // namespace Mizu
