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

    m_counter_pool.resize(m_capacity);
    m_counter_pool_idx = 0;

    for (uint32_t i = 0; i < m_counter_pool.size(); ++i)
    {
        m_counter_pool[i] = new Counter{};
        m_counter_pool[i]->completed = true;
        m_counter_pool[i]->depending_counter = 0;
    }
}

JobSystem::~JobSystem()
{
    kill();

    for (uint32_t i = 0; i < m_counter_pool.size(); ++i)
    {
        delete m_counter_pool[i];
    }
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

JobSystemHandle JobSystem::schedule(const Job& job)
{
    JobSystemHandle handle;
    handle.counter = get_available_counter();

    WorkerJob worker_job{};
    worker_job.func = job.get_function();
    worker_job.handle = handle;

    const ThreadAffinity affinity = job.get_affinity();
    if (affinity == ThreadAffinity_None)
    {
        push_into_jobs_queue(m_global_jobs, worker_job);
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
        push_into_jobs_queue(local_info.local_jobs, worker_job);
    }

    m_wake_condition.notify_one();

    return handle;
}

void JobSystem::kill()
{
    m_alive_fence.reset();

    do
    {
        m_wake_condition.notify_all();

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
    WorkerJob job;

    while (m_alive_fence.is_signaled())
    {
        // Try to get job from local queue; if there are none, try to get from global queue
        if (info.local_jobs.pop(job) || m_global_jobs.pop(job))
        {
            info.is_sleeping = false;

            job.func();

            job.handle.counter->completed.store(true);
            job.handle.counter->completed.notify_all();

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

void JobSystem::push_into_jobs_queue(ThreadSafeRingBuffer<WorkerJob>& job_queue, const WorkerJob& job)
{
    while (!job_queue.push(job))
    {
        poll();
    }
}

Counter* JobSystem::get_available_counter()
{
    Counter* counter = nullptr;

    do
    {
        uint32_t counter_idx = m_counter_pool_idx.fetch_add(1);
        counter_idx = counter_idx % m_counter_pool.size();

        uint32_t old_value = m_counter_pool_idx.load();
        uint32_t new_value = old_value % m_counter_pool.size();
        while (!m_counter_pool_idx.compare_exchange_weak(old_value, new_value, std::memory_order_relaxed))
            new_value = old_value % m_counter_pool.size();

        counter = m_counter_pool[counter_idx];
    } while (!counter->completed);

    counter->completed.store(false);
    counter->depending_counter.store(0);

    return counter;
}

void JobSystem::poll()
{
    m_wake_condition.notify_one();
    std::this_thread::yield();
}

} // namespace Mizu