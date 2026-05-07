#include "core/job_system/job_system.h"

#include <cstdio>
#include <thread>
#include <utility>

#include "base/debug/logging.h"
#include "base/debug/profiling.h"

namespace Mizu
{

JobHandle::JobHandle(const JobHandle& other)
    : completion_index(other.completion_index)
    , generation(other.generation)
    , owner(other.owner)
{
    if (owner == nullptr)
        return;

    CompletionRecord* completion_record = owner->try_get_job_handle_completion_record(*this);
    if (completion_record == nullptr)
        return;

    completion_record->references.fetch_add(1, std::memory_order_relaxed);
}

JobHandle& JobHandle::operator=(const JobHandle& other)
{
    if (this == &other)
        return *this;

    this->~JobHandle();
    new (this) JobHandle(other);

    return *this;
}

JobHandle::JobHandle(JobHandle&& other)
    : completion_index(std::exchange(other.completion_index, IntrusiveFreeListInvalidIndex))
    , generation(other.generation)
    , owner(std::exchange(other.owner, nullptr))
{
}

JobHandle::~JobHandle()
{
    if (owner == nullptr)
        return;

    CompletionRecord* completion_record = owner->try_get_job_handle_completion_record(*this);
    if (completion_record == nullptr)
        return;

    MIZU_ASSERT(
        completion_record->references.load(std::memory_order_relaxed) != 0,
        "JobHandle has completion record that has no references");

    const uint32_t references = completion_record->references.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (references == 0)
    {
        owner->free_completion_record(*completion_record);
    }
}

// Needs to be here because it needs to know that JobSystem is a complete type to be able to call submit_internal
JobHandle PendingJob::submit()
{
    m_submitted = true;
    return m_owner->submit_internal(std::move(*this));
}

// Needs to be here because it needs to know that JobSystem is a complete type to be able to call submit_internal
JobHandle PendingBatch::submit()
{
    m_submitted = true;
    return m_owner->submit_internal(std::move(*this));
}

static constexpr uint32_t MainWorkerId = 0;
thread_local uint32_t s_worker_id = std::numeric_limits<uint32_t>::max();
thread_local size_t s_external_submission_round_robin = 0;

JobSystem::~JobSystem()
{
    MIZU_VERIFY(
        m_num_workers_alive.load(std::memory_order_relaxed) == 0,
        "All worker threads should be dead before destroying the JobSystem");
}

bool JobSystem::init(uint32_t num_workers, bool reserve_main_thread)
{
    MIZU_ASSERT(num_workers > 0, "Must have at least one worker thread");

    m_small_fiber_stack_pool.init(PoolCapacity, get_stack_bytes(StackSize::Small));
    m_medium_fiber_stack_pool.init(PoolCapacity, get_stack_bytes(StackSize::Medium));
    m_large_fiber_stack_pool.init(PoolCapacity, get_stack_bytes(StackSize::Large));

    const uint32_t num_cores = std::thread::hardware_concurrency();
    num_workers = std::min(num_workers, num_cores);

    m_workers.resize(num_workers);

    m_is_enabled.store(true, std::memory_order_relaxed);
    m_main_thread_reserved = reserve_main_thread;

    for (uint32_t i = 0; i < num_workers; i++)
    {
        WorkerInfo& info = m_workers[i];
        info.idx = i;

        // idx == 0 is reserved for the main thread
        if (i == MainWorkerId && reserve_main_thread)
            continue;

        m_num_workers_alive.fetch_add(1, std::memory_order_relaxed);

        std::thread worker_thread(&JobSystem::worker_job, this, std::ref(info));
        worker_thread.detach();
    }

    return true;
}

void JobSystem::attach_as_main_worker()
{
    m_num_workers_alive.fetch_add(1, std::memory_order_relaxed);

    WorkerInfo& main_thread_info = m_workers[MainWorkerId];
    worker_job(main_thread_info);
}

void JobSystem::wait_for(JobHandle handle)
{
    if (is_valid_worker_id(s_worker_id))
    {
        // Fiber path
        WorkerInfo& info = get_thread_worker_info();

        FiberSlot& running_fiber_slot = get_fiber_slot(info.running_fiber_slot_index, info.running_fiber_stack_size);
        MIZU_ASSERT(
            running_fiber_slot.pool_index != IntrusiveFreeListInvalidIndex, "Current fiber slot index is invalid");

        CompletionRecord* dep_completion_record = try_get_job_handle_completion_record(handle);
        MIZU_ASSERT(
            dep_completion_record != nullptr,
            "Trying to wait for an invalid JobHandle in a fiber, no completion record found");

        if (dep_completion_record->is_completed.load(std::memory_order_acquire))
            return;

        JobRecord* try_job_record = try_get_job_record(running_fiber_slot.job_record_ref);
        MIZU_ASSERT(try_job_record != nullptr, "Current fiber slot has invalid JobRecordRef");

        JobRecord& job_record = *try_job_record;
        job_record.state.store(JobState::WaitingRequested, std::memory_order_relaxed);
        job_record.num_remaining_dependencies.fetch_add(1, std::memory_order_acq_rel);
        job_record.waiting_on_completion_index = handle.completion_index;
        job_record.waiting_on_completion_generation = handle.generation;

        MIZU_PROFILE_FIBER_LEAVE;
        fiber_switch(running_fiber_slot.fiber_handle, info.worker_fiber);
    }
    else
    {
        // Blocking fallback path
        wait_for_blocking(handle);
    }
}

bool JobSystem::wait_for_blocking(JobHandle handle)
{
    CompletionRecord* completion_record = try_get_job_handle_completion_record(handle);
    if (completion_record == nullptr)
        return false;

    while (!completion_record->is_completed.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }

    return true;
}

void JobSystem::kill()
{
    m_is_enabled.store(false, std::memory_order_relaxed);
}

void JobSystem::wait_workers_dead()
{
    kill();

    while (m_num_workers_alive.load(std::memory_order_relaxed) > 0)
    {
        std::this_thread::yield();
    }
}

void JobSystem::worker_job(WorkerInfo& info)
{
    s_worker_id = info.idx;

    constexpr size_t FairnessDrainBatchSize = 4;
    constexpr size_t DrainBatchSize = 16;

    info.worker_fiber = fiber_convert_thread_to_fiber();

    JobRecordRef job_record_ref{};
    while (m_is_enabled.load(std::memory_order_relaxed))
    {
        // Drain fairness amount of incoming jobs
        drain_incoming_queue(info, FairnessDrainBatchSize);

        // Try to get from local queue first
        if (info.local_queue.pop(job_record_ref))
        {
            execute_job(job_record_ref);
            continue;
        }

        // If the local queue is empty, drain a batch of jobs and try to get a job from the local queue
        if (drain_incoming_queue(info, DrainBatchSize) > 0 && info.local_queue.pop(job_record_ref))
        {
            execute_job(job_record_ref);
            continue;
        }

        // If there are no jobs in the incoming queue, try to steal
        if (try_steal_job(info, job_record_ref))
        {
            execute_job(job_record_ref);
            continue;
        }

        // TODO: Revisit what is the best way to do this, should the thread sleep?
        std::this_thread::yield();
    }

    fiber_revert_fiber_to_thread(info.worker_fiber);

    m_num_workers_alive.fetch_sub(1, std::memory_order_relaxed);
}

size_t JobSystem::drain_incoming_queue(WorkerInfo& info, size_t batch_size)
{
    size_t count = 0;

    JobRecordRef job_record_ref{};
    while (count < batch_size && info.incoming_queue.pop(job_record_ref))
    {
        info.local_queue.push(job_record_ref);
        count += 1;
    }

    return count;
}

bool JobSystem::try_steal_job(WorkerInfo& info, JobRecordRef& out_record_ref)
{
    // Don't steal from yourself
    if (info.steal_id % m_workers.size() == info.idx)
        info.steal_id += 1;

    // HACK: Don't steal from main worker
    if (m_main_thread_reserved && info.steal_id % m_workers.size() == MainWorkerId)
        info.steal_id += 1;

    const size_t steal_id = info.steal_id++ % m_workers.size();

    WorkerInfo& steal_info = m_workers[steal_id];
    return steal_info.local_queue.steal(out_record_ref);
}

void JobSystem::execute_job(const JobRecordRef& job_record_ref)
{
    JobRecord* try_job = try_get_job_record(job_record_ref);
    if (try_job == nullptr)
    {
        // TODO: Should probably decrement counter and all that fun stuff
        MIZU_ASSERT(false, "Trying to execute a job with an invalid JobRecordRef");
        return;
    }

    JobRecord& job_record = *try_job;

    // TODO: could be unsafe
    CompletionRecord& completion_record = m_completion_record_pool.get(job_record.completion_index);

    FiberSlot* try_fiber_slot = try_get_fiber_slot(job_record.fiber_slot_index, job_record.stack_size);
    if (try_fiber_slot == nullptr)
    {
        // TODO: Should probably decrement counter and all that fun stuff
        MIZU_ASSERT(false, "Trying to execute a job with an invalid FiberSlot index");
        return;
    }

    FiberSlot& fiber_slot = *try_fiber_slot;

    job_record.state.store(JobState::Running, std::memory_order_relaxed);

    WorkerInfo& worker_info = get_thread_worker_info();
    worker_info.running_fiber_slot_index = fiber_slot.pool_index;
    worker_info.running_fiber_stack_size = fiber_slot.stack_size;

    MIZU_PROFILE_FIBER_ENTER(fiber_slot.fiber_name);
    fiber_switch(worker_info.worker_fiber, fiber_slot.fiber_handle);

    if (job_record.state.load(std::memory_order_relaxed) != JobState::Finished)
    {
        // We can't enqueue the suspended from the wait_for call because the job could be picked up betwee  the
        // wait node and fiber switching back to the worker trampoline function. Therefore we wait until the fiber
        // switch has been called (therefore the state stored) and make the fiber trampoline responsible for enqueing
        // the wait node if the state is still WaitingRequested.
        if (job_record.state.load(std::memory_order_relaxed) == JobState::WaitingRequested)
        {
            finalize_requested_job_suspend(job_record);
        }

        // The task yielded via wait_for() and may already have been re-enqueued by this worker or another worker.
        // Only the fiber trampoline marks the task as Finished when the user function actually returns.
        return;
    }

    worker_info.running_fiber_slot_index = IntrusiveFreeListInvalidIndex;

    const uint32_t counter = completion_record.counter.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (counter == 0)
    {
        process_wait_nodes(completion_record);
        completion_record.is_completed.store(true, std::memory_order_release);
    }

    const uint32_t references = completion_record.references.fetch_sub(1, std::memory_order_acq_rel) - 1;
    if (references == 0)
    {
        free_completion_record(completion_record);
    }

    free_job_record(job_record);
    free_fiber_slot(fiber_slot);
}

void JobSystem::execute_fiber(void* info)
{
    JobRecord* job_record_ptr = reinterpret_cast<JobRecord*>(info);
    MIZU_ASSERT(job_record_ptr != nullptr, "Fiber execution info is null");

    JobSystem* job_system = job_record_ptr->owner;

    FiberSlot* fiber_slot =
        job_system->try_get_fiber_slot(job_record_ptr->fiber_slot_index, job_record_ptr->stack_size);
    MIZU_ASSERT(fiber_slot != nullptr, "FiberSlot for the executing fiber is invalid");

    job_record_ptr->func();
    job_record_ptr->state.store(JobState::Finished, std::memory_order_relaxed);

    WorkerInfo& worker_info = job_record_ptr->owner->get_thread_worker_info();

    MIZU_PROFILE_FIBER_LEAVE;
    fiber_switch(fiber_slot->fiber_handle, worker_info.worker_fiber);

    MIZU_UNREACHABLE("Must have returned to worker main loop");
}

void JobSystem::finalize_requested_job_suspend(JobRecord& job_record)
{
    MIZU_ASSERT(
        job_record.state.load(std::memory_order_relaxed) == JobState::WaitingRequested,
        "Tried to finalize a job suspend that was not requested");
    MIZU_ASSERT(
        job_record.waiting_on_completion_index != IntrusiveFreeListInvalidIndex,
        "Requested job suspend is missing its target completion index");

    CompletionRecord& dep_completion_record = m_completion_record_pool.get(job_record.waiting_on_completion_index);
    MIZU_ASSERT(
        dep_completion_record.generation == job_record.waiting_on_completion_generation,
        "Requested job suspend is waiting on a stale completion record");

    const auto resume_job_immediately = [&]() {
        job_record.waiting_on_completion_index = IntrusiveFreeListInvalidIndex;
        job_record.waiting_on_completion_generation = 0;

        const uint32_t remaining_dependencies =
            job_record.num_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;
        MIZU_ASSERT(remaining_dependencies == 0, "Requested job suspend resumed with unexpected dependencies");

        job_record.state.store(JobState::Ready, std::memory_order_relaxed);
        enqueue_job_record(job_record);
    };

    if (dep_completion_record.is_completed.load(std::memory_order_acquire))
    {
        resume_job_immediately();
        return;
    }

    WaitNode& wait_node = allocate_wait_node();
    wait_node.type = WaitNodeType::SuspendedTask;
    wait_node.target_job_index = job_record.pool_index;

    job_record.state.store(JobState::WaitingParked, std::memory_order_relaxed);
    if (!try_push_wait_node(dep_completion_record, wait_node))
    {
        free_wait_node(wait_node);
        resume_job_immediately();
    }
}

void JobSystem::process_wait_nodes(CompletionRecord& completion_record)
{
    IntrusiveFreeListIndex wait_node_index =
        completion_record.waiting_jobs_head.exchange(CompletionRecord::ClosedWaitingList, std::memory_order_acq_rel);

    IntrusiveFreeListIndex current_index = wait_node_index;
    while (current_index != IntrusiveFreeListInvalidIndex)
    {
        WaitNode& wait_node = m_wait_node_pool.get(current_index);

        if (wait_node.type == WaitNodeType::DependencyEdge)
        {
            JobRecord& dependent_job = m_job_record_pool.get(wait_node.target_job_index);

            const uint32_t remaining_deps =
                dependent_job.num_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;

            if (remaining_deps == 0)
            {
                dependent_job.state.store(JobState::Ready, std::memory_order_relaxed);
                enqueue_job_record(dependent_job);
            }
        }
        else if (wait_node.type == WaitNodeType::SuspendedTask)
        {
            JobRecord& waiting_job = m_job_record_pool.get(wait_node.target_job_index);
            MIZU_ASSERT(
                waiting_job.state.load(std::memory_order_relaxed) == JobState::WaitingParked,
                "WaitNode is trying to resume a job that has not finished parking");

            const uint32_t remaining_deps =
                waiting_job.num_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;

            if (remaining_deps == 0)
            {
                waiting_job.state.store(JobState::Ready, std::memory_order_relaxed);
                waiting_job.waiting_on_completion_index = IntrusiveFreeListInvalidIndex;
                waiting_job.waiting_on_completion_generation = 0;
                enqueue_job_record(waiting_job);
            }
        }
        else
        {
            MIZU_UNREACHABLE("Not implemented");
        }

        current_index = wait_node.next;

        // Free the wait node after processing it
        free_wait_node(wait_node);
    }
}

JobHandle JobSystem::submit_internal(PendingJob&& job)
{
    CompletionRecord& completion_record = allocate_completion_record();
    JobRecord& job_record = allocate_job_record();
    FiberSlot& fiber_slot = allocate_fiber_slot(job.m_desc.m_stack_size);

    init_completion_record(completion_record, 1);
    init_fiber_slot(fiber_slot, job_record, job.m_desc);
    init_job_record(job.m_desc, job_record, completion_record, fiber_slot);

    submit_job_record_internal(job_record, job.m_desc, job.m_dependencies);

    JobHandle handle{};
    handle.completion_index = completion_record.pool_index;
    handle.generation = completion_record.generation;
    handle.owner = this;

    completion_record.references.fetch_add(1, std::memory_order_relaxed);

    return handle;
}

JobHandle JobSystem::submit_internal(PendingBatch&& batch)
{
    CompletionRecord& completion_record = allocate_completion_record();
    init_completion_record(completion_record, static_cast<uint32_t>(batch.m_jobs.size()));

    for (JobDescription& desc : batch.m_jobs)
    {
        JobRecord& job_record = allocate_job_record();
        FiberSlot& fiber_slot = allocate_fiber_slot(desc.m_stack_size);

        init_fiber_slot(fiber_slot, job_record, desc);
        init_job_record(desc, job_record, completion_record, fiber_slot);

        submit_job_record_internal(job_record, desc, batch.m_dependencies);
    }

    // If the batch is empty, set as completed already and reset is_completed and waiting_job_head.
    // counter and references will already be 0 as m_jobs.size() == 0
    if (batch.m_jobs.empty())
    {
        completion_record.is_completed.store(true, std::memory_order_relaxed);
        completion_record.waiting_jobs_head.store(CompletionRecord::ClosedWaitingList, std::memory_order_relaxed);
    }

    JobHandle handle{};
    handle.completion_index = completion_record.pool_index;
    handle.generation = completion_record.generation;
    handle.owner = this;

    completion_record.references.fetch_add(1, std::memory_order_relaxed);

    return handle;
}

void JobSystem::submit_job_record_internal(
    JobRecord& job_record,
    [[maybe_unused]] const JobDescription& job_desc,
    std::span<JobHandle> dependencies)
{
    // Start with a submission sentinel so dependency completions cannot drive the counter to zero
    // until all visible dependency edges have been published.
    job_record.num_remaining_dependencies.store(1, std::memory_order_relaxed);

    for (const JobHandle& job_dependency : dependencies)
    {
        CompletionRecord* dep_completion_record = try_get_job_handle_completion_record(job_dependency);
        if (dep_completion_record == nullptr)
        {
            MIZU_LOG_ERROR("Trying to schedule job '{}' with invalid dependency", job_desc.m_name);
            continue;
        }

        if (dep_completion_record->is_completed.load(std::memory_order_acquire))
            continue;

        WaitNode& wait_node = allocate_wait_node();
        wait_node.type = WaitNodeType::DependencyEdge;
        wait_node.target_job_index = job_record.pool_index;

        job_record.num_remaining_dependencies.fetch_add(1, std::memory_order_relaxed);

        if (!try_push_wait_node(*dep_completion_record, wait_node))
        {
            // Completion closed, release allocated wait node
            free_wait_node(wait_node);
            job_record.num_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel);
            continue;
        }
    }

    job_record.state.store(JobState::PendingDependencies, std::memory_order_relaxed);

    const uint32_t remaining_dependencies =
        job_record.num_remaining_dependencies.fetch_sub(1, std::memory_order_acq_rel) - 1;

    if (remaining_dependencies == 0)
    {
        job_record.state.store(JobState::Ready, std::memory_order_relaxed);
        enqueue_job_record(job_record);
    }
}

void JobSystem::init_job_record(
    JobDescription& job,
    JobRecord& job_record,
    const CompletionRecord& completion_record,
    const FiberSlot& fiber_slot)
{
    // Can't set `state` and `num_remaining_dependencies` here because the dependency jobs may have already finished
    job_record.state.store(JobState::PendingDependencies, std::memory_order_relaxed);
    job_record.func = std::move(job.m_func);
    job_record.affinity = job.m_affinity;
    job_record.num_remaining_dependencies.store(0, std::memory_order_relaxed);
    job_record.completion_index = completion_record.pool_index;
    job_record.fiber_slot_index = fiber_slot.pool_index;
    job_record.stack_size = fiber_slot.stack_size;
    job_record.waiting_on_completion_index = IntrusiveFreeListInvalidIndex;
    job_record.waiting_on_completion_generation = 0;

    job_record.owner = this;
}

void JobSystem::init_completion_record(CompletionRecord& completion_record, uint32_t counter)
{
    completion_record.is_completed.store(false, std::memory_order_relaxed);
    completion_record.counter.store(counter, std::memory_order_relaxed);
    completion_record.waiting_jobs_head.store(IntrusiveFreeListInvalidIndex, std::memory_order_relaxed);
    completion_record.references.store(counter, std::memory_order_relaxed);
}

void JobSystem::init_fiber_slot(FiberSlot& fiber_slot, JobRecord& job_record, const JobDescription& job_desc)
{
    fiber_slot.job_record_ref = JobRecordRef{job_record.pool_index, job_record.generation};
    fiber_slot.stack_size = job_desc.m_stack_size;

#if MIZU_DEBUG
    std::snprintf(fiber_slot.fiber_name, FiberSlot::FiberNameMaxLength, "fiber-%u", fiber_slot.pool_index);
#endif

    FiberStackMemoryPool& stack_pool = get_fiber_stack_memory_pool(fiber_slot.stack_size);
    uint8_t* stack_memory = stack_pool.get_memory(fiber_slot.pool_index);

    fiber_slot.fiber_handle = fiber_create(
        stack_memory,
        get_stack_bytes(fiber_slot.stack_size),
        &JobSystem::execute_fiber,
        reinterpret_cast<void*>(&job_record));
}

template <typename T>
concept IsQueue = requires(T t, JobRecordRef value) {
    { t.push(value) } -> std::same_as<bool>;
};

template <typename T>
    requires IsQueue<T>
static bool try_enqueue_job_record(T& queue, JobRecordRef value, uint32_t num_attempts)
{
    uint32_t attempts = 0;
    while (attempts < num_attempts)
    {
        if (queue.push(value))
            return true;

        attempts += 1;
        std::this_thread::yield();
    }

    return false;
}

void JobSystem::enqueue_job_record(const JobRecord& job_record)
{
    static constexpr uint32_t MaxEnqueueAttempts = 128;

    const JobAffinity affinity = job_record.affinity;

    JobRecordRef job_record_ref{};
    job_record_ref.index = job_record.pool_index;
    job_record_ref.generation = job_record.generation;

    if (affinity == JobAffinity::Main && s_worker_id != MainWorkerId && is_valid_worker_id(s_worker_id))
    {
        WorkerInfo& main_thread_info = m_workers[0];
        // main_thread_info.incoming_queue.push(job_record_ref);
        MIZU_VERIFY(
            try_enqueue_job_record(main_thread_info.incoming_queue, job_record_ref, MaxEnqueueAttempts),
            "Failed to enqueue job");
    }
    else if (affinity != JobAffinity::Any && affinity != JobAffinity::Main)
    {
        // TODO: Should send to one of the workers with the specified affinity
        MIZU_UNREACHABLE("Not implemented");
    }
    else if (is_valid_worker_id(s_worker_id))
    {
        WorkerInfo& worker_info = get_thread_worker_info();
        // worker_info.local_queue.push(job_record_ref);
        MIZU_VERIFY(
            try_enqueue_job_record(worker_info.local_queue, job_record_ref, MaxEnqueueAttempts),
            "Failed to enqueue job");
    }
    else
    {
        // Trying to submit from a thread that is not a worker, distribute to random worker via round-robin
        WorkerInfo& worker_info = m_workers[s_external_submission_round_robin++ % m_workers.size()];
        MIZU_VERIFY(
            try_enqueue_job_record(worker_info.incoming_queue, job_record_ref, MaxEnqueueAttempts),
            "Failed to enqueue job");
    }
}

JobRecord* JobSystem::try_get_job_record(const JobRecordRef& job_record_ref)
{
    if (!job_record_ref.is_valid())
        return nullptr;

    JobRecord& record = m_job_record_pool.get(job_record_ref.index);
    if (record.generation != job_record_ref.generation)
        return nullptr;

    return &record;
}

JobRecord& JobSystem::get_job_record(const JobRecordRef& job_record_ref)
{
    JobRecord* record = try_get_job_record(job_record_ref);
    MIZU_ASSERT(record != nullptr, "Invalid JobRecordRef");
    return *record;
}

CompletionRecord* JobSystem::try_get_job_handle_completion_record(const JobHandle& handle)
{
    if (!handle.is_valid())
        return nullptr;

    if (handle.owner != this)
        return nullptr;

    CompletionRecord& record = m_completion_record_pool.get(handle.completion_index);
    if (record.generation != handle.generation)
        return nullptr;

    return &record;
}

CompletionRecord& JobSystem::get_job_handle_completion_record(const JobHandle& handle)
{
    CompletionRecord* record = try_get_job_handle_completion_record(handle);
    MIZU_ASSERT(record != nullptr, "Invalid job handle");
    return *record;
}

bool JobSystem::try_push_wait_node(CompletionRecord& completion_record, WaitNode& wait_node)
{
    while (true)
    {
        IntrusiveFreeListIndex head = completion_record.waiting_jobs_head.load(std::memory_order_acquire);
        if (head == CompletionRecord::ClosedWaitingList)
            return false; // Completion is closed

        wait_node.next = head;

        if (completion_record.waiting_jobs_head.compare_exchange_weak(
                head, wait_node.pool_index, std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            return true;
        }
    }

    return false;
}

FiberSlot* JobSystem::try_get_fiber_slot(IntrusiveFreeListIndex index, StackSize stack_size)
{
    if (index == IntrusiveFreeListInvalidIndex)
        return nullptr;

    FiberSlotPool& pool = get_fiber_slot_pool(stack_size);

    FiberSlot& slot = pool.get(index);
    if (slot.stack_size != stack_size)
        return nullptr;

    return &slot;
}

FiberSlot& JobSystem::get_fiber_slot(IntrusiveFreeListIndex index, StackSize stack_size)
{
    FiberSlot* slot = try_get_fiber_slot(index, stack_size);
    MIZU_ASSERT(slot != nullptr, "Invalid fiber slot index or stack size");
    return *slot;
}

JobRecord& JobSystem::allocate_job_record()
{
    IntrusiveFreeListIndex index = m_job_record_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "JobRecord allocate failed");

    JobRecord& record = m_job_record_pool.get(index);
    record.generation += 1;

    return record;
}

CompletionRecord& JobSystem::allocate_completion_record()
{
    IntrusiveFreeListIndex index = m_completion_record_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "CompletionRecord allocate failed");

    CompletionRecord& record = m_completion_record_pool.get(index);
    record.generation += 1;

    return record;
}

WaitNode& JobSystem::allocate_wait_node()
{
    IntrusiveFreeListIndex index = m_wait_node_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "WaitNode allocate failed");

    return m_wait_node_pool.get(index);
}

FiberSlot& JobSystem::allocate_fiber_slot(StackSize stack_size)
{
    FiberSlotPool& pool = get_fiber_slot_pool(stack_size);

    const IntrusiveFreeListIndex index = pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "FiberSlot allocate failed");

    FiberSlot& fiber_slot = pool.get(index);
    fiber_slot.stack_size = stack_size;

    return fiber_slot;
}

void JobSystem::free_job_record(JobRecord& job_record)
{
    MIZU_ASSERT(job_record.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid JobRecord");
    job_record.state.store(JobState::Free, std::memory_order_relaxed);
    job_record.waiting_on_completion_index = IntrusiveFreeListInvalidIndex;
    job_record.waiting_on_completion_generation = 0;
    m_job_record_pool.free(job_record.pool_index);
}

void JobSystem::free_completion_record(const CompletionRecord& completion_record)
{
    MIZU_ASSERT(
        completion_record.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid CompletionRecord");
    m_completion_record_pool.free(completion_record.pool_index);
}

void JobSystem::free_wait_node(const WaitNode& wait_node)
{
    MIZU_ASSERT(wait_node.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid WaitNode");
    m_wait_node_pool.free(wait_node.pool_index);
}

void JobSystem::free_fiber_slot(FiberSlot& fiber_slot)
{
    MIZU_ASSERT(fiber_slot.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid FiberSlot");

    fiber_destroy(fiber_slot.fiber_handle);

    FiberSlotPool& pool = get_fiber_slot_pool(fiber_slot.stack_size);
    pool.free(fiber_slot.pool_index);
}

JobSystem::FiberSlotPool& JobSystem::get_fiber_slot_pool(StackSize stack_size)
{
    switch (stack_size)
    {
    case StackSize::Small:
        return m_small_fiber_pool;
    case StackSize::Medium:
        return m_medium_fiber_pool;
    case StackSize::Large:
        return m_large_fiber_pool;
    }
}

FiberStackMemoryPool& JobSystem::get_fiber_stack_memory_pool(StackSize stack_size)
{
    switch (stack_size)
    {
    case StackSize::Small:
        return m_small_fiber_stack_pool;
    case StackSize::Medium:
        return m_medium_fiber_stack_pool;
    case StackSize::Large:
        return m_large_fiber_stack_pool;
    }
}

size_t JobSystem::get_stack_bytes(StackSize stack_size) const
{
    // TODO: MAKE THIS SIZES CONFIGURABLE BY USER
    switch (stack_size)
    {
    case StackSize::Small:
        return 32 * 1024; // 32 KB
    case StackSize::Medium:
        return 128 * 1024; // 128 KB
    case StackSize::Large:
        return 512 * 1024; // 512 KB
    }
}

bool JobSystem::is_valid_worker_id(uint32_t worker_id) const
{
    return worker_id < m_workers.size();
}

JobSystem::WorkerInfo& JobSystem::get_thread_worker_info()
{
    MIZU_ASSERT(is_valid_worker_id(s_worker_id), "Invalid worker index for this thread");
    return m_workers[s_worker_id];
}

} // namespace Mizu
