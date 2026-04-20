#include "core/job_system/job_system.h"

#include "base/debug/logging.h"

namespace Mizu
{

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

bool JobSystem2::init(uint32_t num_workers)
{
    (void)num_workers;
    return false;
}

void JobSystem2::worker_job() {}

JobHandle2 JobSystem2::submit_internal(PendingJob&& job)
{
    CompletionRecord& completion_record = allocate_completion_record();
    JobRecord& job_record = allocate_job_record();

    init_completion_record(completion_record, 1);
    init_job_record(job.m_desc, job_record, completion_record);

    for (const JobHandle2& job_dependency : job.m_dependencies)
    {
        CompletionRecord* dep_completion_record = try_get_job_handle_completion_record(job_dependency);
        if (dep_completion_record == nullptr)
        {
            MIZU_LOG_ERROR("Trying to schedule job '{}' with invalid dependency", job.m_desc.m_name);
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

        job_record.num_remaining_dependencies += 1;
    }

    job_record.state = job_record.num_remaining_dependencies == 0 ? JobState::Ready : JobState::PendingDependencies;

    JobHandle2 handle{};
    handle.completion_index = completion_record.pool_index;
    handle.generation = completion_record.generation;
    handle.owner = this;

    return handle;
}

JobHandle2 JobSystem2::submit_internal(PendingBatch&& batch)
{
    (void)batch;
    MIZU_UNREACHABLE("Not implemented");
    return JobHandle2();
}

void JobSystem2::init_job_record(JobDescription& job, JobRecord& job_record, const CompletionRecord& completion_record)
{
    // Can't set state and num_remaining_dependencies here because the dependency jobs may have already finished
    job_record.state = JobState::PendingDependencies;
    job_record.func = std::move(job.m_func);
    job_record.num_remaining_dependencies = 0;
    job_record.completion_index = completion_record.pool_index;
}

void JobSystem2::init_completion_record(CompletionRecord& completion_record, uint32_t counter)
{
    completion_record.is_completed.store(false, std::memory_order_relaxed);
    completion_record.counter.store(counter, std::memory_order_relaxed);
    completion_record.waiting_jobs_head.store(IntrusiveFreeListInvalidIndex, std::memory_order_relaxed);
    completion_record.references = 1;
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

    return m_job_record_pool.get(index);
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

void JobSystem2::free_wait_node(const WaitNode& wait_node)
{
    MIZU_ASSERT(wait_node.pool_index != IntrusiveFreeListInvalidIndex, "Trying to free invalid WaitNode");
    m_wait_node_pool.free(wait_node.pool_index);
}

} // namespace Mizu
