#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <condition_variable>

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

        return job;
    }

    Job& set_thread_affinity(ThreadAffinity affinity)
    {
        m_affinity = affinity;
        return *this;
    }

    JobSystemFunction get_function() const { return m_function; }
    ThreadAffinity get_thread_affinity() const { return m_affinity; }

  private:
    JobSystemFunction m_function;
    ThreadAffinity m_affinity{ThreadAffinity_None};
};

class JobSystem
{
  public:
    JobSystem(uint32_t num_threads, size_t capacity);

    void init();

    void execute(const Job& job);

    void wait_and_stop();
    void kill();

  private:
    struct WorkerLocalInfo
    {
        uint32_t thread_idx;
        ThreadSafeRingBuffer<JobSystemFunction>* local_jobs;
    };

    uint32_t m_num_threads;
    size_t m_capacity;

    ThreadFence m_alive_fence{true};

    std::mutex m_wake_mutex;
    std::condition_variable m_wake_condition;

    std::atomic<uint32_t> m_num_workers_alive;

    std::vector<WorkerLocalInfo> m_worker_infos;
    ThreadSafeRingBuffer<JobSystemFunction> m_global_jobs;

    void worker_job(WorkerLocalInfo info);

    void push_into_jobs_queue(ThreadSafeRingBuffer<JobSystemFunction>& jobs, const JobSystemFunction& function);

    void poll();
};

} // namespace Mizu