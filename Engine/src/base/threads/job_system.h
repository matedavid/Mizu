#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>

#include "base/threads/fence.h"
#include "base/threads/ring_buffer.h"

namespace Mizu
{

using ThreadAffinity = uint8_t;
constexpr ThreadAffinity ThreadAffinity_None = std::numeric_limits<ThreadAffinity>::max();

using JobSystemFunction = std::function<void()>;

struct Counter;


struct Continuation
{
    JobSystemFunction func{};
    ThreadAffinity affinity = ThreadAffinity_None;
    Counter* counter = nullptr;
};

struct Counter
{
    std::atomic<bool> completed = true;
    std::atomic<uint32_t> execution_counter = 0;
    std::atomic<uint32_t> dependencies_counter = 0;

    std::array<Continuation, 20> continuations;
    std::atomic<uint32_t> num_continuations = 0;
};

struct JobSystemHandle
{
    Counter* counter = nullptr;

    inline void wait() const
    {
        if (counter->completed.load())
            return;

        counter->completed.wait(false);
    }
};

class Job
{
  public:
    template <typename Func, typename... Args>
    static Job create(const Func& func, Args... args)
    {
        Job job{};
        job.m_function = [func, args...]() mutable {
            func(args...);
        };
        job.m_affinity = ThreadAffinity_None;

        return job;
    }

    Job& set_affinity(ThreadAffinity affinity)
    {
        m_affinity = affinity;
        return *this;
    }

    Job& depends_on(JobSystemHandle handle)
    {
        m_depends_on.push_back(handle);
        return *this;
    }

    JobSystemFunction get_function() const { return m_function; }
    ThreadAffinity get_affinity() const { return m_affinity; }
    bool has_dependencies() const { return !m_depends_on.empty(); }
    const std::vector<JobSystemHandle>& get_dependencies() const { return m_depends_on; }
    inline void execute() const { m_function(); }

  private:
    JobSystemFunction m_function;
    ThreadAffinity m_affinity{ThreadAffinity_None};
    std::vector<JobSystemHandle> m_depends_on;
};

class JobSystem
{
  public:
    JobSystem(uint32_t num_threads, size_t capacity);
    ~JobSystem();

    void init();

    JobSystemHandle schedule(const Job& job);
    JobSystemHandle schedule(std::span<Job> jobs);
    void run_thread_as_worker(ThreadAffinity affinity);

    void kill();
    void wait_workers_are_dead();

  private:
    struct WorkerJob
    {
        JobSystemFunction func;
        ThreadAffinity affinity;
        JobSystemHandle handle;
    };

    struct WorkerLocalInfo
    {
        ThreadSafeRingBuffer<WorkerJob> local_jobs{};
        uint32_t worker_id = 0;
        bool is_sleeping = false;
    };

    uint32_t m_num_workers;
    size_t m_capacity;

    ThreadFence m_alive_fence{true};
    std::atomic<uint32_t> m_num_workers_alive;

    std::mutex m_wake_mutex;
    std::condition_variable m_wake_condition;

    ThreadSafeRingBuffer<WorkerJob> m_global_jobs;
    std::vector<WorkerLocalInfo> m_worker_infos;

    std::vector<Counter*> m_counter_pool;
    std::atomic<uint32_t> m_counter_pool_idx;

    void worker_job(WorkerLocalInfo& info);
    void finish_job(const WorkerJob& worker_job);

    void push_job(const WorkerJob& worker_job);
    void push_into_jobs_queue(ThreadSafeRingBuffer<WorkerJob>& job_queue, const WorkerJob& job);

    Counter* get_available_counter();
    void poll();
};

} // namespace Mizu