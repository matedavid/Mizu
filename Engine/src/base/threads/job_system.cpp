#include "job_system.h"

#include <thread>

#include "base/debug/assert.h"

namespace Mizu
{

JobSystem::JobSystem(uint32_t num_threads, size_t capacity)
{
    MIZU_ASSERT(num_threads != 0 && capacity != 0, "Can't request 0 threads or a capacity of 0");

    const uint32_t num_cores = std::thread::hardware_concurrency();
    m_num_workers = std::min(num_cores, num_threads);

#if MIZU_DEBUG
    if (m_num_workers != num_threads)
    {
        MIZU_LOG_WARNING(
            "Decreasing the number of workers from the requested {} threads to {} because that's the number of cores "
            "in the machine",
            m_num_workers,
            num_cores);
    }
#endif

    m_capacity = capacity;

    m_global_jobs.init(m_capacity);
    m_worker_infos.resize(m_num_workers);
}

void JobSystem::init()
{
    // TODO: -1 because thread 0 is not a worker, for the moment
    m_num_workers_alive.store(m_num_workers - 1);

    // TODO: Doing because thread 0 is not a worker, and therefore does not set the sleeping flag
    m_worker_infos[0].is_sleeping = true;

    // TODO: Starting from 1 because thread 0 is not a worker, for the moment
    for (uint32_t i = 1; i < m_num_workers; ++i)
    {
        constexpr size_t LOCAL_JOB_QUEUE_CAPACITY = 10u;

        WorkerLocalInfo& local_info = m_worker_infos[i];
        local_info.local_jobs.init(LOCAL_JOB_QUEUE_CAPACITY);
        local_info.is_sleeping = false;

        std::thread worker(&JobSystem::worker_job, this, std::ref(local_info));
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
        push_into_jobs_queue(local_info.local_jobs, job.get_function());
    }

    m_wake_condition.notify_one();
}

void JobSystem::wait_all_jobs_finished() const
{
    const auto is_busy = [this]() -> bool {
        for (const WorkerLocalInfo& info : m_worker_infos)
        {
            if (!info.is_sleeping)
                return true;
        }

        return false;
    };

    while (is_busy())
    {
        std::this_thread::yield();
    }
}

void JobSystem::kill()
{
    m_alive_fence.reset();

    do
    {
        const uint32_t current = m_num_workers_alive.load();
        m_num_workers_alive.wait(current);
    } while (m_num_workers_alive.load() != 0);

    m_global_jobs.reset();

    for (WorkerLocalInfo& info : m_worker_infos)
    {
        info.local_jobs.reset();
    }
}

void JobSystem::worker_job(WorkerLocalInfo& info)
{
    JobSystemFunction job;

    while (m_alive_fence.is_signaled())
    {
        // Try to get job from local queue
        if (info.local_jobs.pop(job))
        {
            info.is_sleeping = false;

            job();
            continue;
        }

        // If there is no job in the local queue, try to get from global queue
        if (m_global_jobs.pop(job))
        {
            info.is_sleeping = false;

            job();
            continue;
        }

        info.is_sleeping = true;

        // If there is no job available, sleep until woken up
        std::unique_lock<std::mutex> lock(m_wake_mutex);
        m_wake_condition.wait(lock);
    }

    m_num_workers_alive.fetch_sub(1);
    m_num_workers_alive.notify_all();
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