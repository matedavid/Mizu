#pragma once

#include <atomic>
#include <deque>
#include <functional>
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

struct JobRecordPoolTag;
struct CompletionRecordPoolTag;
struct WaitNodePoolTag;
struct FiberSlotPoolTag;

using JobRecordPoolIndex = IntrusiveFreeListIndex<JobRecordPoolTag>;
using CompletionRecordPoolIndex = IntrusiveFreeListIndex<CompletionRecordPoolTag>;
using WaitNodePoolIndex = IntrusiveFreeListIndex<WaitNodePoolTag>;
using FiberSlotPoolIndex = IntrusiveFreeListIndex<FiberSlotPoolTag>;

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

class JobSystem;

struct MIZU_CORE_API JobHandle
{
    CompletionRecordPoolIndex completion_index{};
    size_t generation = 0;
    JobSystem* owner = nullptr;

    JobHandle() = default;
    JobHandle(const JobHandle& other);
    JobHandle& operator=(const JobHandle& other);
    JobHandle(JobHandle&& other);

    ~JobHandle();

    bool is_valid() const { return completion_index.is_valid(); }
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

    JobDescription&& name(std::string_view name) &&
    {
        m_name = name;
        return std::move(*this);
    }

    JobDescription&& affinity(JobAffinity affinity) &&
    {
        m_affinity = affinity;
        return std::move(*this);
    }

    JobDescription&& stack_size(StackSize stack_size) &&
    {
        m_stack_size = stack_size;
        return std::move(*this);
    }

  private:
    InplaceJobFunction m_func{};
    JobAffinity m_affinity = JobAffinity::Any;
    StackSize m_stack_size = StackSize::Medium;

    [[maybe_unused]] std::string_view m_name{};

    friend class PendingJob;
    friend class JobSystem;
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

    PendingJob&& stack_size(StackSize stack_size)
    {
        m_desc.m_stack_size = stack_size;
        return std::move(*this);
    }

    PendingJob& depends_on(JobHandle handle)
    {
        m_dependencies.push_back(handle);
        return *this;
    }

    JobHandle submit();

  private:
    JobSystem* m_owner = nullptr;

    JobDescription m_desc{};
    inplace_vector<JobHandle, MaxJobDependencies> m_dependencies{};

    bool m_submitted = false;

    template <typename Func, typename... Args>
    PendingJob(JobSystem* owner, Func&& func, Args&&... args) : m_owner(owner)
    {
        m_desc = JobDescription{}.create(func, args...);
    }

    friend class JobSystem;
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

    PendingBatch& depends_on(JobHandle handle)
    {
        m_dependencies.push_back(handle);
        return *this;
    }

    JobHandle submit();

  private:
    JobSystem* m_owner = nullptr;

    static constexpr size_t MaxBatchJobs = 64;
    inplace_vector<JobDescription, MaxBatchJobs> m_jobs{};
    inplace_vector<JobHandle, MaxJobDependencies> m_dependencies{};

    bool m_submitted = false;

    PendingBatch(JobSystem* owner) : m_owner(owner) {}

    friend class JobSystem;
};

enum class JobState
{
    Free,
    PendingDependencies,
    Ready,
    Running,
    WaitingRequested,
    WaitingParked,
    Finished,
};

enum class WaitNodeType
{
    DependencyEdge,
    SuspendedTask,
    CounterWait,
};

template <typename IndexT>
struct IntrusiveFreeListRecordBase
{
    IndexT pool_index{};
    IndexT next_free{};
};

struct JobRecord : public IntrusiveFreeListRecordBase<JobRecordPoolIndex>
{
    std::atomic<JobState> state = JobState::Free;
    InplaceJobFunction func{};
    JobAffinity affinity = JobAffinity::Any;

    std::atomic<uint32_t> num_remaining_dependencies = 0;
    CompletionRecordPoolIndex completion_index{};
    FiberSlotPoolIndex fiber_slot_index{};
    StackSize stack_size = StackSize::Medium;

    // For suspension during execution (wait_for)
    CompletionRecordPoolIndex waiting_on_completion_index{};
    size_t waiting_on_completion_generation = 0;

    uint32_t generation = 0;
    JobSystem* owner = nullptr;
};

struct CompletionRecord : public IntrusiveFreeListRecordBase<CompletionRecordPoolIndex>
{
    std::atomic<bool> is_completed = false;
    // Number of jobs that need to finish before we consider this CompletionRecord completed.
    std::atomic<uint32_t> counter = 0;

    // max()-1 differentiates the closed waiting-list sentinel from the default invalid index; the default invalid index
    // means the list is empty.
    static constexpr WaitNodePoolIndex ClosedWaitingList =
        WaitNodePoolIndex{std::numeric_limits<typename WaitNodePoolIndex::ValueT>::max() - 1};
    std::atomic<WaitNodePoolIndex> waiting_jobs_head = WaitNodePoolIndex{};

    std::atomic<uint32_t> references = 0;
    uint32_t generation = 0;
};

struct WaitNode : public IntrusiveFreeListRecordBase<WaitNodePoolIndex>
{
    WaitNodeType type{};
    JobRecordPoolIndex target_job_index{};
    WaitNodePoolIndex next{};

    // For resuming jobs, prefer the same worker it was already being executed in for cache locality purposes.
    // It's not a guarantee, just a hint.
    uint32_t preferred_resume_worker_id = 0;
};

#define MIZU_CREATE_INTRUSIVE_FREE_LIST_REF(_name) \
    struct _name                                   \
    {                                              \
        JobRecordPoolIndex index{};                \
        size_t generation = 0;                     \
                                                   \
        bool is_valid() const                      \
        {                                          \
            return index.is_valid();               \
        }                                          \
    }

MIZU_CREATE_INTRUSIVE_FREE_LIST_REF(JobRecordRef);

#undef MIZU_CREATE_INTRUSIVE_FREE_LIST_REF

struct FiberSlot : public IntrusiveFreeListRecordBase<FiberSlotPoolIndex>
{
    StackSize stack_size = StackSize::Medium;
    JobRecordRef job_record_ref = JobRecordRef{};
    FiberHandle fiber_handle{};

    static constexpr size_t FiberNameMaxLength = 32;
    [[maybe_unused]] char fiber_name[FiberNameMaxLength] = {};
};

class MIZU_CORE_API JobSystem
{
  public:
    JobSystem() = default;
    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;
    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    ~JobSystem();

    bool init(uint32_t num_workers, bool reserve_main_thread = true);
    void attach_as_main_worker();

    void wait_for(JobHandle handle);
    bool wait_for_blocking(JobHandle handle);

    void kill();
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

        FiberSlotPoolIndex running_fiber_slot_index{};
        StackSize running_fiber_stack_size = StackSize::Medium;

        WorkStealingDeque<JobRecordRef, WorkerQueueCapacity> local_queue{};
        MpscQueue<JobRecordRef, WorkerQueueCapacity> incoming_queue{};
    };

    std::deque<WorkerInfo> m_workers;
    bool m_main_thread_reserved = false;

    using JobRecordPool = IntrusiveFreeList<JobRecord, PoolCapacity, JobRecordPoolTag>;
    using CompletionRecordPool = IntrusiveFreeList<CompletionRecord, PoolCapacity, CompletionRecordPoolTag>;
    using WaitNodePool = IntrusiveFreeList<WaitNode, PoolCapacity, WaitNodePoolTag>;
    using FiberSlotPool = IntrusiveFreeList<FiberSlot, PoolCapacity, FiberSlotPoolTag>;

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
    void finalize_requested_job_suspend(JobRecord& job_record);
    void process_wait_nodes(CompletionRecord& completion_record);

    JobHandle submit_internal(PendingJob&& job);
    JobHandle submit_internal(PendingBatch&& batch);
    void submit_job_record_internal(
        JobRecord& job_record,
        const JobDescription& job_desc,
        std::span<JobHandle> dependencies);

    void init_job_record(
        JobDescription& job,
        JobRecord& job_record,
        const CompletionRecord& completion_record,
        const FiberSlot& fiber_slot);
    void init_completion_record(CompletionRecord& completion_record, uint32_t counter);
    void init_fiber_slot(FiberSlot& fiber_slot, JobRecord& job_record, const JobDescription& job_desc);

    void enqueue_job_record(const JobRecord& job_record);

    JobRecord* try_get_job_record(const JobRecordRef& job_record_ref);
    JobRecord& get_job_record(const JobRecordRef& job_record_ref);
    CompletionRecord* try_get_job_handle_completion_record(const JobHandle& handle);
    CompletionRecord& get_job_handle_completion_record(const JobHandle& handle);
    bool try_push_wait_node(CompletionRecord& completion_record, WaitNode& wait_node);
    FiberSlot* try_get_fiber_slot(FiberSlotPoolIndex index, StackSize stack_size);
    FiberSlot& get_fiber_slot(FiberSlotPoolIndex index, StackSize stack_size);

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

    friend struct JobHandle;
    friend class PendingJob;
    friend class PendingBatch;
};

} // namespace Mizu
