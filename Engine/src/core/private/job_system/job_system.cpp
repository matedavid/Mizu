#include "core/job_system/job_system.h"

#include <thread>

#include "base/debug/logging.h"

namespace Mizu
{

JobHandle2::JobHandle2(const JobHandle2& other)
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

JobHandle2& JobHandle2::operator=(const JobHandle2& other)
{
    if (this == &other)
        return *this;

    this->~JobHandle2();
    new (this) JobHandle2(other);

    return *this;
}

JobHandle2::~JobHandle2()
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

// Needs to be here because it needs to know that JobSystem2 is a complete type to be able to call submit_internal
JobHandle2 PendingJob::submit()
{
    m_submitted = true;
    return m_owner->submit_internal(std::move(*this));
}

// Needs to be here because it needs to know that JobSystem2 is a complete type to be able to call submit_internal
JobHandle2 PendingBatch::submit()
{
    m_submitted = true;
    return m_owner->submit_internal(std::move(*this));
}

static constexpr uint32_t MainWorkerId = 0;
thread_local uint32_t s_worker_id = std::numeric_limits<uint32_t>::max();
thread_local size_t s_external_submission_round_robin = 0;

JobSystem2::~JobSystem2()
{
    MIZU_VERIFY(
        m_num_workers_alive.load(std::memory_order_relaxed) == 0,
        "All worker threads should be dead before destroying the JobSystem2");
}

bool JobSystem2::init(uint32_t num_workers, bool reserve_main_thread)
{
    MIZU_ASSERT(num_workers > 0, "Must have at least one worker thread");

    const uint32_t num_cores = std::thread::hardware_concurrency();
    num_workers = std::min(num_workers, num_cores);

    m_workers.resize(num_workers);

    m_is_enabled.store(true, std::memory_order_relaxed);

    for (uint32_t i = 0; i < num_workers; i++)
    {
        WorkerInfo& info = m_workers[i];
        info.idx = i;

        // idx == 0 is reserved for the main thread
        if (i == MainWorkerId && reserve_main_thread)
            continue;

        m_num_workers_alive.fetch_add(1, std::memory_order_relaxed);

        std::thread worker_thread(&JobSystem2::worker_job, this, std::ref(info));
        worker_thread.detach();
    }

    return true;
}

void JobSystem2::attach_as_main_worker()
{
    m_num_workers_alive.fetch_add(1, std::memory_order_relaxed);

    WorkerInfo& main_thread_info = m_workers[MainWorkerId];
    worker_job(main_thread_info);
}

bool JobSystem2::wait_for_blocking(JobHandle2 handle)
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

void JobSystem2::wait_workers_dead()
{
    m_is_enabled.store(false, std::memory_order_relaxed);

    while (m_num_workers_alive.load(std::memory_order_relaxed) > 0)
    {
        std::this_thread::yield();
    }
}

void JobSystem2::worker_job(WorkerInfo& info)
{
    s_worker_id = info.idx;

    constexpr size_t FairnessDrainBatchSize = 4;
    constexpr size_t DrainBatchSize = 16;

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

    m_num_workers_alive.fetch_sub(1, std::memory_order_relaxed);
}

size_t JobSystem2::drain_incoming_queue(WorkerInfo& info, size_t batch_size)
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

bool JobSystem2::try_steal_job(WorkerInfo& info, JobRecordRef& out_record_ref)
{
    // Don't steal from yourself
    if (info.steal_id % m_workers.size() == info.idx)
        info.steal_id += 1;

    size_t steal_id = info.steal_id++ % m_workers.size();

    WorkerInfo& steal_info = m_workers[steal_id];
    return steal_info.local_queue.steal(out_record_ref);
}

void JobSystem2::execute_job(const JobRecordRef& job_record_ref)
{
    JobRecord* try_job = try_get_job_record(job_record_ref);
    if (try_job == nullptr)
    {
        MIZU_LOG_ERROR("Trying to execute a job with an invalid JobRecordRef");
        return;
    }

    JobRecord& job_record = *try_job;
    // TODO: could be unsafe
    CompletionRecord& completion_record = m_completion_record_pool.get(job_record.completion_index);

    job_record.func();

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
}

void JobSystem2::process_wait_nodes(CompletionRecord& completion_record)
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
                dependent_job.state = JobState::Ready;
                enqueue_job_record(dependent_job);
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

JobHandle2 JobSystem2::submit_internal(PendingJob&& job)
{
    CompletionRecord& completion_record = allocate_completion_record();
    JobRecord& job_record = allocate_job_record();

    init_completion_record(completion_record, 1);
    init_job_record(job.m_desc, job_record, completion_record);

    submit_job_record_internal(job_record, job.m_desc, job.m_dependencies);

    JobHandle2 handle{};
    handle.completion_index = completion_record.pool_index;
    handle.generation = completion_record.generation;
    handle.owner = this;

    completion_record.references.fetch_add(1, std::memory_order_relaxed);

    return handle;
}

JobHandle2 JobSystem2::submit_internal(PendingBatch&& batch)
{
    CompletionRecord& completion_record = allocate_completion_record();
    init_completion_record(completion_record, static_cast<uint32_t>(batch.m_jobs.size()));

    for (JobDescription& desc : batch.m_jobs)
    {
        JobRecord& job_record = allocate_job_record();
        init_job_record(desc, job_record, completion_record);

        submit_job_record_internal(job_record, desc, batch.m_dependencies);
    }

    // If the batch is empty, set as completed already and reset is_completed and waiting_job_head.
    // counter and references will already be 0 as m_jobs.size() == 0
    if (batch.m_jobs.empty())
    {
        completion_record.is_completed.store(true, std::memory_order_relaxed);
        completion_record.waiting_jobs_head.store(CompletionRecord::ClosedWaitingList, std::memory_order_relaxed);
    }

    JobHandle2 handle{};
    handle.completion_index = completion_record.pool_index;
    handle.generation = completion_record.generation;
    handle.owner = this;

    completion_record.references.fetch_add(1, std::memory_order_relaxed);

    return handle;
}

void JobSystem2::submit_job_record_internal(
    JobRecord& job_record,
    const JobDescription& job_desc,
    std::span<JobHandle2> dependencies)
{
    uint32_t remaining_dependencies = 0;
    for (const JobHandle2& job_dependency : dependencies)
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

        if (!try_push_wait_node(*dep_completion_record, wait_node))
        {
            // Completion closed, release allocated wait node
            free_wait_node(wait_node);
            continue;
        }

        remaining_dependencies += 1;
    }

    job_record.num_remaining_dependencies.store(remaining_dependencies, std::memory_order_relaxed);
    job_record.state = job_record.num_remaining_dependencies == 0 ? JobState::Ready : JobState::PendingDependencies;

    if (job_record.state == JobState::Ready)
    {
        enqueue_job_record(job_record);
    }
}

void JobSystem2::init_job_record(JobDescription& job, JobRecord& job_record, const CompletionRecord& completion_record)
{
    // Can't set `state` and `num_remaining_dependencies` here because the dependency jobs may have already finished
    job_record.state = JobState::PendingDependencies;
    job_record.func = std::move(job.m_func);
    job_record.affinity = job.m_affinity;
    job_record.num_remaining_dependencies.store(0, std::memory_order_relaxed);
    job_record.completion_index = completion_record.pool_index;
}

void JobSystem2::init_completion_record(CompletionRecord& completion_record, uint32_t counter)
{
    completion_record.is_completed.store(false, std::memory_order_relaxed);
    completion_record.counter.store(counter, std::memory_order_relaxed);
    completion_record.waiting_jobs_head.store(IntrusiveFreeListInvalidIndex, std::memory_order_relaxed);
    completion_record.references.store(counter, std::memory_order_relaxed);
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

void JobSystem2::enqueue_job_record(const JobRecord& job_record)
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

JobRecord* JobSystem2::try_get_job_record(const JobRecordRef& job_record_ref)
{
    if (!job_record_ref.is_valid())
        return nullptr;

    JobRecord& record = m_job_record_pool.get(job_record_ref.index);
    if (record.generation != job_record_ref.generation)
        return nullptr;

    return &record;
}

JobRecord& JobSystem2::get_job_record(const JobRecordRef& job_record_ref)
{
    JobRecord* record = try_get_job_record(job_record_ref);
    MIZU_ASSERT(record != nullptr, "Invalid JobRecordRef");
    return *record;
}

CompletionRecord* JobSystem2::try_get_job_handle_completion_record(const JobHandle2& handle)
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

CompletionRecord& JobSystem2::get_job_handle_completion_record(const JobHandle2& handle)
{
    CompletionRecord* record = try_get_job_handle_completion_record(handle);
    MIZU_ASSERT(record != nullptr, "Invalid job handle");
    return *record;
}

bool JobSystem2::try_push_wait_node(CompletionRecord& completion_record, WaitNode& wait_node)
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

JobRecord& JobSystem2::allocate_job_record()
{
    IntrusiveFreeListIndex index = m_job_record_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "JobRecord allocate failed");

    JobRecord& record = m_job_record_pool.get(index);
    record.generation += 1;

    return record;
}

CompletionRecord& JobSystem2::allocate_completion_record()
{
    IntrusiveFreeListIndex index = m_completion_record_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "CompletionRecord allocate failed");

    CompletionRecord& record = m_completion_record_pool.get(index);
    record.generation += 1;

    return record;
}

WaitNode& JobSystem2::allocate_wait_node()
{
    IntrusiveFreeListIndex index = m_wait_node_pool.allocate();
    MIZU_ASSERT(index != IntrusiveFreeListInvalidIndex, "WaitNode allocate failed");

    return m_wait_node_pool.get(index);
}

void JobSystem2::free_job_record(const JobRecord& job_record)
{
    MIZU_ASSERT(job_record.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid JobRecord");
    m_job_record_pool.free(job_record.pool_index);
}

void JobSystem2::free_completion_record(const CompletionRecord& completion_record)
{
    MIZU_ASSERT(
        completion_record.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid CompletionRecord");
    m_completion_record_pool.free(completion_record.pool_index);
}

void JobSystem2::free_wait_node(const WaitNode& wait_node)
{
    MIZU_ASSERT(wait_node.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid WaitNode");
    m_wait_node_pool.free(wait_node.pool_index);
}

bool JobSystem2::is_valid_worker_id(uint32_t worker_id) const
{
    return worker_id < m_workers.size();
}

JobSystem2::WorkerInfo& JobSystem2::get_thread_worker_info()
{
    MIZU_ASSERT(is_valid_worker_id(s_worker_id), "Invalid worker index for this thread");
    return m_workers[s_worker_id];
}
} // namespace Mizu
