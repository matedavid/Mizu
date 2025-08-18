#include "job_system.h"

#include <thread>

#include "base/debug/assert.h"

namespace Mizu
{

JobSystem::JobSystem(uint32_t num_threads, size_t capacity)
    : m_num_threads(num_threads)
    , m_capacity(capacity)
    , m_global_jobs(m_capacity)
{
}

void JobSystem::init()
{
    MIZU_ASSERT(m_num_threads != 0, "Can't request 0 threads");

    const uint32_t num_cores = std::thread::hardware_concurrency();
    const uint32_t num_workers = std::min(num_cores, m_num_threads);

#if MIZU_DEBUG
    if (num_workers != m_num_threads)
    {
        MIZU_LOG_WARNING(
            "Decreasing the number of workers from the requested {} threads to {} because that's the number of cores "
            "in the machine",
            m_num_threads,
            num_workers);
    }
#endif

    m_worker_infos.resize(num_workers);
    m_num_workers_alive.store(num_workers);

    WorkerLocalInfo local_info{};
    for (uint32_t i = 1; i < num_workers; ++i)
    {
        local_info.thread_idx = i;
        local_info.local_jobs = new ThreadSafeRingBuffer<JobSystemFunction>(10);

        m_worker_infos[i] = local_info;

        std::thread worker(&JobSystem::worker_job, this, m_worker_infos[i]);
        worker.detach();
    }
}

void JobSystem::execute(const Job& job)
{
    const ThreadAffinity affinity = job.get_thread_affinity();

    if (affinity == ThreadAffinity_None)
    {
        push_into_jobs_queue(m_global_jobs, job.get_function());
    }
    else
    {
        MIZU_ASSERT(
            affinity < m_worker_infos.size(),
            "Invalid affinity value {}, there are only {} workers",
            affinity,
            m_worker_infos.size());

        // TODO: TEMPORARY
        MIZU_ASSERT(affinity != 0, "Affinity 0 not implemented :)");

        WorkerLocalInfo& local_info = m_worker_infos[affinity];
        push_into_jobs_queue(*local_info.local_jobs, job.get_function());
    }

    m_wake_condition.notify_one();
}

void JobSystem::wait_and_stop()
{
    // TODO: Implement function that waits for all threads to finish executing functions in the queues

    while (true)
    {
    }
}

void JobSystem::kill()
{
    m_alive_fence.reset();

    while (m_num_workers_alive.load() != 1)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(500));
    }
}

void JobSystem::worker_job(WorkerLocalInfo info)
{
    JobSystemFunction job;

    while (m_alive_fence.is_signaled())
    {
        // Try to get job from local queue
        if (info.local_jobs->pop(job))
        {
            job();
            continue;
        }

        // If there is no job in the local queue, try to get from global queue
        if (m_global_jobs.pop(job))
        {
            job();
            continue;
        }

        // If there is no job available, sleep until woken up
        std::unique_lock<std::mutex> lock(m_wake_mutex);
        m_wake_condition.wait(lock);
    }

    m_num_workers_alive.fetch_sub(1);
}

void JobSystem::push_into_jobs_queue(ThreadSafeRingBuffer<JobSystemFunction>& jobs, const JobSystemFunction& function)
{
    while (!jobs.push(function))
    {
        poll();
    }
}

void JobSystem::poll()
{
    m_wake_condition.notify_one();
    std::this_thread::yield();
}

} // namespace Mizu