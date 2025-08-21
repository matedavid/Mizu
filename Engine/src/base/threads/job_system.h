#pragma once

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <optional>

#include "base/threads/fence.h"
#include "base/threads/ring_buffer.h"

namespace Mizu
{

using ThreadAffinity = uint8_t;
constexpr ThreadAffinity ThreadAffinity_None = std::numeric_limits<ThreadAffinity>::max();

using JobSystemFunction = std::function<void()>;

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

    JobSystemFunction get_function() const { return m_function; }
    ThreadAffinity get_affinity() const { return m_affinity; }

  private:
    JobSystemFunction m_function;
    ThreadAffinity m_affinity{ThreadAffinity_None};
};

class JobSystem
{
  public:
    JobSystem(uint32_t num_threads, size_t capacity);

    void init();

    void schedule(const Job& job);

    void wait_all_jobs_finished() const;
    void kill();

  private:
    struct WorkerLocalInfo
    {
        ThreadSafeRingBuffer<JobSystemFunction> local_jobs{};
        bool is_sleeping = false;
    };

    uint32_t m_num_workers;
    size_t m_capacity;

    ThreadFence m_alive_fence{true};
    std::atomic<uint32_t> m_num_workers_alive;

    std::mutex m_wake_mutex;
    std::condition_variable m_wake_condition;

    ThreadSafeRingBuffer<JobSystemFunction> m_global_jobs;
    std::vector<WorkerLocalInfo> m_worker_infos;

    void worker_job(WorkerLocalInfo& info);
    void push_into_jobs_queue(ThreadSafeRingBuffer<JobSystemFunction>& jobs, const JobSystemFunction& function);
    void poll();
};

} // namespace Mizu