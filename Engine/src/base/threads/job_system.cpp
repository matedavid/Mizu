#include "job_system.h"

#include <format>
#include <thread>

#include "base/debug/assert.h"
#include "base/debug/profiling.h"

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

    for (uint32_t i = 0; i < m_num_workers; ++i)
    {
        m_worker_infos[i] = new WorkerLocalInfo{};
    }

    m_counter_pool.resize(m_capacity);
    m_counter_pool_idx = 0;

    for (uint32_t i = 0; i < m_counter_pool.size(); ++i)
    {
        m_counter_pool[i] = new Counter{};
        m_counter_pool[i]->completed = true;
        m_counter_pool[i]->execution_counter = 0;
        m_counter_pool[i]->dependencies_counter = 0;
    }
}

JobSystem::~JobSystem()
{
    kill();
    wait_workers_are_dead();

    for (uint32_t i = 0; i < m_worker_infos.size(); ++i)
    {
        delete m_worker_infos[i];
    }

    for (uint32_t i = 0; i < m_counter_pool.size(); ++i)
    {
        delete m_counter_pool[i];
    }
}

void JobSystem::init()
{
    m_num_workers_alive.store(m_num_workers - 1);
    m_next_worker_to_wake_idx = 0;

    for (uint32_t i = 0; i < m_num_workers; ++i)
    {
        constexpr size_t LOCAL_JOB_QUEUE_CAPACITY = 10u;

        WorkerLocalInfo& local_info = *m_worker_infos[i];
        local_info.local_jobs.init(LOCAL_JOB_QUEUE_CAPACITY);
        local_info.worker_id = i;
        local_info.is_sleeping = false;

        if (i == 0)
            continue;

        std::thread worker(&JobSystem::worker_job, this, std::ref(local_info));
        worker.detach();
    }
}

JobSystemHandle JobSystem::schedule(const Job& job)
{
    MIZU_ASSERT(m_num_workers_alive.load() != 0, "There are no workers alive, did you call init()?");

    Counter* counter = get_available_counter();
    counter->completed.store(false);
    counter->execution_counter.store(1);
    counter->dependencies_counter.store(0);
    counter->num_continuations.store(0);

    JobSystemHandle handle{};
    handle.counter = counter;

    if (job.has_dependencies())
    {
        std::lock_guard lock(counter->continuations_mutex);

        uint32_t num_dependencies = 0;

        for (const JobSystemHandle& dependency : job.get_dependencies())
        {
            MIZU_ASSERT(dependency.counter != nullptr, "Dependency handle has null counter");

            MIZU_ASSERT(
                dependency.counter->num_continuations < dependency.counter->continuations.size(),
                "Job handle does not support more continuations... already has {} continuations",
                dependency.counter->num_continuations.load());

            Continuation continuation{};
            continuation.func = job.get_function();
            continuation.affinity = job.get_affinity();
            continuation.counter = counter;

            if (dependency.counter->completed.load(std::memory_order_acquire))
                continue;

            const uint32_t continuation_idx = dependency.counter->num_continuations.fetch_add(1);
            MIZU_ASSERT(
                continuation_idx < dependency.counter->continuations.size(), "Continuation index out of bounds");
            dependency.counter->continuations[continuation_idx] = continuation;

            num_dependencies += 1;
        }

        counter->dependencies_counter.store(num_dependencies);
    }

    if (counter->dependencies_counter > 0)
        return handle;

    WorkerJob worker_job{};
    worker_job.func = job.get_function();
    worker_job.affinity = job.get_affinity();
    worker_job.handle = handle;

    push_job(worker_job);

    return handle;
}

JobSystemHandle JobSystem::schedule(std::span<Job> jobs)
{
    MIZU_ASSERT(m_num_workers_alive.load() != 0, "There are no workers alive, did you call init()?");

    const uint32_t num_jobs = static_cast<uint32_t>(jobs.size());

    Counter* counter = get_available_counter();
    counter->completed.store(false);
    counter->execution_counter.store(num_jobs);
    counter->dependencies_counter.store(0);
    counter->num_continuations.store(0);

    JobSystemHandle handle{};
    handle.counter = counter;

    // TODO: Dependencies not implemented for multiple job scheduling

    for (const Job& job : jobs)
    {
        WorkerJob worker_job{};
        worker_job.func = job.get_function();
        worker_job.affinity = job.get_affinity();
        worker_job.handle = handle;

        push_job(worker_job);
    }

    return handle;
}

void JobSystem::run_thread_as_worker(ThreadAffinity affinity)
{
    m_num_workers_alive.fetch_add(1);

    WorkerLocalInfo& local_info = *m_worker_infos[affinity];
    worker_job(local_info);
}

void JobSystem::kill()
{
    if (!m_alive_fence.is_signaled())
        return;

    m_alive_fence.reset();

    for (WorkerLocalInfo* info : m_worker_infos)
    {
        info->wake_condition.notify_all();
    }
}

void JobSystem::wait_workers_are_dead() const
{
    MIZU_ASSERT(
        !m_alive_fence.is_signaled(),
        "To wait for all workers to be dead you must have previously killed the workers (by calling kill())");

    for (WorkerLocalInfo* info : m_worker_infos)
    {
        info->wake_condition.notify_all();
    }

    while (true)
    {
        const uint32_t current = m_num_workers_alive.load(std::memory_order_acquire);
        if (current == 0)
            break;

        m_num_workers_alive.wait(current);
    }
}

void JobSystem::worker_job(WorkerLocalInfo& info)
{
#if MIZU_DEBUG
    const std::string worker_name = std::format("Worker {}", info.worker_id);
    MIZU_PROFILE_SET_THREAD_NAME(worker_name.c_str());
#endif

    WorkerJob job;

    while (m_alive_fence.is_signaled())
    {
        // Try to get job from local queue; if there are none, try to get from global queue
        if (info.local_jobs.pop(job) || m_global_jobs.pop(job))
        {
            info.is_sleeping = false;

            job.func();

            finish_job(job);

            continue;
        }

        info.is_sleeping = true;

        // If there is no job available, sleep until woken up
        std::unique_lock lock(info.wake_mutex);
        info.wake_condition.wait(
            lock, [&]() { return !m_alive_fence.is_signaled() || !info.local_jobs.empty() || !m_global_jobs.empty(); });
    }

    m_num_workers_alive.fetch_sub(1);
    m_num_workers_alive.notify_all();
}

void JobSystem::finish_job(const WorkerJob& job)
{
    Counter* counter = job.handle.counter;

    const uint32_t prev_execution_counter = counter->execution_counter.fetch_sub(1, std::memory_order_relaxed);
    counter->execution_counter.notify_all();

    if (prev_execution_counter - 1 == 0)
    {
        std::lock_guard lock(counter->continuations_mutex);

        counter->completed.store(true);
        counter->completed.notify_all();

        for (uint32_t i = 0; i < counter->num_continuations; ++i)
        {
            const Continuation& continuation = counter->continuations[i];

            std::lock_guard lock2(continuation.counter->continuations_mutex);

            const uint32_t prev_dependencies_counter = continuation.counter->dependencies_counter.fetch_sub(1);

            if (prev_dependencies_counter - 1 == 0)
            {
                WorkerJob worker_job{};
                worker_job.func = continuation.func;
                worker_job.affinity = continuation.affinity;
                worker_job.handle = JobSystemHandle{.counter = continuation.counter};

                push_job(worker_job);
            }
        }
    }
}

void JobSystem::push_job(const WorkerJob& worker_job)
{
    const ThreadAffinity affinity = worker_job.affinity;
    if (affinity == ThreadAffinity_None)
    {
        push_into_jobs_queue(m_global_jobs, worker_job);

#if MIZU_DEBUG
        const uint32_t starting_wake_idx = m_next_worker_to_wake_idx.load(std::memory_order_acquire);
#endif

        while (true)
        {
            const uint32_t worker_to_wake_idx =
                m_next_worker_to_wake_idx.fetch_add(1, std::memory_order_relaxed) % m_worker_infos.size();

            MIZU_ASSERT(
                (worker_to_wake_idx + 1) % m_worker_infos.size() != starting_wake_idx,
                "There are no available workers to be woken up, did a full circle");

            WorkerLocalInfo& local_info = *m_worker_infos[worker_to_wake_idx];
            if (local_info.is_sleeping)
            {
                std::lock_guard lock(m_worker_infos[worker_to_wake_idx]->wake_mutex);
                m_worker_infos[worker_to_wake_idx]->wake_condition.notify_one();

                break;
            }

            std::this_thread::yield();
        }
    }
    else
    {
        MIZU_ASSERT(
            affinity < m_worker_infos.size(),
            "Invalid affinity value {}, there are only {} workers",
            affinity,
            m_worker_infos.size());

        WorkerLocalInfo& local_info = *m_worker_infos[affinity];
        push_into_jobs_queue(local_info.local_jobs, worker_job);

        {
            std::lock_guard lock(local_info.wake_mutex);
            local_info.wake_condition.notify_one();
        }
    }
}

void JobSystem::push_into_jobs_queue(ThreadSafeRingBuffer<WorkerJob>& job_queue, const WorkerJob& worker_job)
{
    while (!job_queue.push(worker_job))
    {
        std::this_thread::yield();
    }
}

Counter* JobSystem::get_available_counter()
{
    while (true)
    {
        const uint32_t counter_idx = m_counter_pool_idx.fetch_add(1, std::memory_order_relaxed) % m_counter_pool.size();
        Counter* counter = m_counter_pool[counter_idx];

        if (counter->completed.load(std::memory_order_acquire))
        {
            return counter;
        }

        std::this_thread::yield();
    }
}

} // namespace Mizu