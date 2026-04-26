#pragma once

#include <atomic>
#include <deque>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"

#include "core/job_system/fiber_stack_memory_pool.h"
#include "core/job_system/fibers.h"
#include "core/job_system/inplace_job_function.h"
#include "core/job_system/intrusive_free_list.h"
#include "core/job_system/mpsc_queue.h"
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

enum class StackSize
{
    Small,
    Medium,
    Large,
};

class JobSystem2;

struct MIZU_CORE_API JobHandle2
{
    IntrusiveFreeListIndex completion_index = IntrusiveFreeListInvalidIndex;
    size_t generation = 0;
    JobSystem2* owner = nullptr;

    JobHandle2() = default;
    JobHandle2(const JobHandle2& other);
    JobHandle2& operator=(const JobHandle2& other);

    ~JobHandle2();

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

class MIZU_CORE_API PendingJob
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

class MIZU_CORE_API PendingBatch
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
    JobAffinity affinity = JobAffinity::Any;

    std::atomic<uint32_t> num_remaining_dependencies = 0;
    IntrusiveFreeListIndex completion_index = IntrusiveFreeListInvalidIndex;
    IntrusiveFreeListIndex fiber_slot_index = IntrusiveFreeListInvalidIndex;
    StackSize stack_size = StackSize::Small;

    // For suspension during execution (wait_for)
    IntrusiveFreeListIndex waiting_on_completion_index = IntrusiveFreeListInvalidIndex;

    uint32_t generation = 0;
    JobSystem2* owner = nullptr;
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

#define MIZU_CREATE_INTRUSIVE_FREE_LIST_REF(_name)                    \
    struct _name                                                      \
    {                                                                 \
        IntrusiveFreeListIndex index = IntrusiveFreeListInvalidIndex; \
        size_t generation = 0;                                        \
                                                                      \
        bool is_valid() const                                         \
        {                                                             \
            return index != IntrusiveFreeListInvalidIndex;            \
        }                                                             \
    }

MIZU_CREATE_INTRUSIVE_FREE_LIST_REF(JobRecordRef);

#undef MIZU_CREATE_INTRUSIVE_FREE_LIST_REF

struct FiberSlot : public IntrusiveFreeListRecordBase
{
    StackSize stack_size = StackSize::Small;
    JobRecordRef job_record_ref = JobRecordRef{};
    FiberHandle fiber_handle{};
};

class MIZU_CORE_API JobSystem2
{
  public:
    ~JobSystem2();

    bool init(uint32_t num_workers, bool reserve_main_thread = true);
    void attach_as_main_worker();

    void wait_for(JobHandle2 handle);
    bool wait_for_blocking(JobHandle2 handle);
    void wait_workers_dead();

    template <typename Func, typename... Args>
    PendingJob schedule(Func&& func, Args&&... args)
    {
        return PendingJob{this, std::forward<Func>(func), std::forward<Args>(args)...};
    }

    PendingBatch schedule_batch() { return PendingBatch{this}; }

  private:
    static constexpr size_t WorkerQueueCapacity = 512;
    static constexpr size_t PoolCapacity = 2048;

    struct WorkerInfo
    {
        uint32_t idx = 0;
        size_t steal_id = 0;

        FiberHandle worker_fiber{};

        IntrusiveFreeListIndex running_fiber_slot_index = IntrusiveFreeListInvalidIndex;
        StackSize running_fiber_stack_size = StackSize::Small;

        WorkStealingDeque<JobRecordRef, WorkerQueueCapacity> local_queue;
        MpscQueue<JobRecordRef, WorkerQueueCapacity> incoming_queue;
    };

    std::deque<WorkerInfo> m_workers;

    using JobRecordPool = IntrusiveFreeList<JobRecord, PoolCapacity>;
    using CompletionRecordPool = IntrusiveFreeList<CompletionRecord, PoolCapacity>;
    using WaitNodePool = IntrusiveFreeList<WaitNode, PoolCapacity>;
    using FiberSlotPool = IntrusiveFreeList<FiberSlot, PoolCapacity>;

    JobRecordPool m_job_record_pool;
    CompletionRecordPool m_completion_record_pool;
    WaitNodePool m_wait_node_pool;
    FiberSlotPool m_small_fiber_pool, m_medium_fiber_pool, m_large_fiber_pool;
    FiberStackMemoryPool m_small_fiber_stack_pool, m_medium_fiber_stack_pool, m_large_fiber_stack_pool;

    std::atomic<bool> m_is_enabled{};
    std::atomic<uint32_t> m_num_workers_alive{};

    void worker_job(WorkerInfo& info);
    size_t drain_incoming_queue(WorkerInfo& info, size_t batch_size);
    bool try_steal_job(WorkerInfo& info, JobRecordRef& out_record_ref);
    void execute_job(const JobRecordRef& job_record_ref);
    static void execute_fiber(void* info);
    void process_wait_nodes(CompletionRecord& completion_record);

    JobHandle2 submit_internal(PendingJob&& job);
    JobHandle2 submit_internal(PendingBatch&& batch);
    void submit_job_record_internal(
        JobRecord& job_record,
        const JobDescription& job_desc,
        std::span<JobHandle2> dependencies);

    void init_job_record(
        JobDescription& job,
        JobRecord& job_record,
        const CompletionRecord& completion_record,
        const FiberSlot& fiber_slot);
    void init_completion_record(CompletionRecord& completion_record, uint32_t counter);
    void init_fiber_slot(FiberSlot& fiber_slot, JobRecord& job_record);

    void enqueue_job_record(const JobRecord& job_record);

    JobRecord* try_get_job_record(const JobRecordRef& job_record_ref);
    JobRecord& get_job_record(const JobRecordRef& job_record_ref);
    CompletionRecord* try_get_job_handle_completion_record(const JobHandle2& handle);
    CompletionRecord& get_job_handle_completion_record(const JobHandle2& handle);
    bool try_push_wait_node(CompletionRecord& completion_record, WaitNode& wait_node);
    FiberSlot* try_get_fiber_slot(IntrusiveFreeListIndex index, StackSize stack_size);
    FiberSlot& get_fiber_slot(IntrusiveFreeListIndex index, StackSize stack_size);

    JobRecord& allocate_job_record();
    CompletionRecord& allocate_completion_record();
    WaitNode& allocate_wait_node();
    FiberSlot& allocate_fiber_slot(StackSize stack_size);

    void free_job_record(JobRecord& job_record);
    void free_completion_record(const CompletionRecord& completion_record);
    void free_wait_node(const WaitNode& wait_node);
    void free_fiber_slot(FiberSlot& fiber_slot);

    FiberSlotPool& get_fiber_slot_pool(StackSize stack_size);
    FiberStackMemoryPool& get_fiber_stack_memory_pool(StackSize stack_size);

    size_t get_stack_bytes(StackSize stack_size) const;
    bool is_valid_worker_id(uint32_t worker_id) const;
    WorkerInfo& get_thread_worker_info();

    friend struct JobHandle2;
    friend class PendingJob;
    friend class PendingBatch;
};

} // namespace Mizu
