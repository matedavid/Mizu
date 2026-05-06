#include <catch2/catch_all.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>
#include <vector>

#include "core/job_system/job_system.h"

using namespace Mizu;

struct JobSystemStressScope
{
    JobSystem job_system;
    bool initialized = false;

    ~JobSystemStressScope()
    {
        if (initialized)
        {
            job_system.wait_workers_dead();
        }
    }
};

TEST_CASE("JobSystem reuses completion records across more than pool capacity jobs", "[JobSystem][stress]")
{
    constexpr int32_t NumJobs = 3000;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;

    for (int32_t index = 0; index < NumJobs; ++index)
    {
        JobHandle handle =
            scope.job_system.schedule([&] { executed.fetch_add(1, std::memory_order_acq_rel); }).submit();

        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumJobs);
}

TEST_CASE("JobSystem reuses completion records across many batches", "[JobSystem][stress]")
{
    constexpr int32_t NumBatches = 512;
    constexpr int32_t JobsPerBatch = 4;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;

    for (int32_t batch_index = 0; batch_index < NumBatches; ++batch_index)
    {
        PendingBatch batch = scope.job_system.schedule_batch();
        for (int32_t job_index = 0; job_index < JobsPerBatch; ++job_index)
        {
            batch.add([&] { executed.fetch_add(1, std::memory_order_acq_rel); });
        }

        JobHandle handle = batch.submit();
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumBatches * JobsPerBatch);
}

TEST_CASE("JobSystem executes a large fan-out of independent jobs", "[JobSystem][stress]")
{
    constexpr int32_t NumJobs = 256;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;
    std::vector<JobHandle> handles;
    handles.reserve(static_cast<size_t>(NumJobs));

    for (int32_t index = 0; index < NumJobs; ++index)
    {
        handles.push_back(
            scope.job_system.schedule([&] { executed.fetch_add(1, std::memory_order_acq_rel); }).submit());
    }

    for (const JobHandle& handle : handles)
    {
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumJobs);
}

TEST_CASE("JobSystem executes a deep dependency chain", "[JobSystem][stress]")
{
    constexpr int32_t ChainLength = 64;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> final_order = 0;
    std::vector<JobHandle> handles;
    handles.reserve(static_cast<size_t>(ChainLength));

    for (int32_t index = 0; index < ChainLength; ++index)
    {
        PendingJob job = scope.job_system.schedule([&, index] {
            const int32_t order = step.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (index == ChainLength - 1)
            {
                final_order.store(order, std::memory_order_release);
            }
        });

        if (!handles.empty())
        {
            job.depends_on(handles.back());
        }

        handles.push_back(job.submit());
    }

    REQUIRE(scope.job_system.wait_for_blocking(handles.back()));
    REQUIRE(final_order.load(std::memory_order_acquire) == ChainLength);
}

TEST_CASE("JobSystem executes multiple batches concurrently", "[JobSystem][stress]")
{
    constexpr int32_t NumBatches = 16;
    constexpr int32_t JobsPerBatch = 8;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;
    std::vector<JobHandle> handles;
    handles.reserve(static_cast<size_t>(NumBatches));

    for (int32_t batch_index = 0; batch_index < NumBatches; ++batch_index)
    {
        PendingBatch batch = scope.job_system.schedule_batch();
        for (int32_t job_index = 0; job_index < JobsPerBatch; ++job_index)
        {
            batch.add([&] { executed.fetch_add(1, std::memory_order_acq_rel); });
        }

        handles.push_back(batch.submit());
    }

    for (const JobHandle& handle : handles)
    {
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumBatches * JobsPerBatch);
}

TEST_CASE("JobSystem rejects stale handles after pool reuse", "[JobSystem][stress]")
{
    constexpr int32_t NumReuseIterations = 3000;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    JobHandle stale_handle{};
    {
        JobHandle handle = scope.job_system.schedule([] {}).submit();
        stale_handle.completion_index = handle.completion_index;
        stale_handle.generation = handle.generation;
        stale_handle.owner = handle.owner;

        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    for (int32_t index = 0; index < NumReuseIterations; ++index)
    {
        JobHandle handle = scope.job_system.schedule([] {}).submit();
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE_FALSE(scope.job_system.wait_for_blocking(stale_handle));
}

TEST_CASE("JobSystem repeatedly resumes in-fiber wait_for under contention", "[JobSystem][stress]")
{
    constexpr int32_t Iterations = 200;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    for (int32_t iteration = 0; iteration < Iterations; ++iteration)
    {
        std::atomic<bool> dependency_can_finish = false;
        std::atomic<bool> dependency_completed = false;
        std::atomic<int32_t> waiters_started = 0;
        std::atomic<int32_t> waiters_resumed = 0;

        constexpr int32_t NumWaiters = 3;
        JobHandle orchestrator =
            scope.job_system
                .schedule([&] {
                    JobHandle dependency = scope.job_system
                                                .schedule([&] {
                                                    while (!dependency_can_finish.load(std::memory_order_acquire))
                                                    {
                                                        std::this_thread::yield();
                                                    }

                                                    dependency_completed.store(true, std::memory_order_release);
                                                })
                                                .submit();

                    std::vector<JobHandle> waiter_handles;
                    waiter_handles.reserve(NumWaiters);

                    for (int32_t index = 0; index < NumWaiters; ++index)
                    {
                        waiter_handles.push_back(scope.job_system
                                                     .schedule([&] {
                                                         waiters_started.fetch_add(1, std::memory_order_acq_rel);
                                                         scope.job_system.wait_for(dependency);

                                                         if (dependency_completed.load(std::memory_order_acquire))
                                                         {
                                                             waiters_resumed.fetch_add(1, std::memory_order_acq_rel);
                                                         }
                                                     })
                                                     .submit());
                    }

                    while (waiters_started.load(std::memory_order_acquire) != NumWaiters)
                    {
                        std::this_thread::yield();
                    }

                    dependency_can_finish.store(true, std::memory_order_release);

                    for (const JobHandle& waiter_handle : waiter_handles)
                    {
                        scope.job_system.wait_for(waiter_handle);
                    }
                })
                .submit();

        REQUIRE(scope.job_system.wait_for_blocking(orchestrator));

        REQUIRE(waiters_resumed.load(std::memory_order_acquire) == NumWaiters);
    }
}

TEST_CASE("JobSystem supports in-fiber wait_for on batch completion handles", "[JobSystem][stress]")
{
    constexpr int32_t Iterations = 128;

    JobSystemStressScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    for (int32_t iteration = 0; iteration < Iterations; ++iteration)
    {
        std::atomic<bool> waiter_started = false;
        std::atomic<bool> waiter_resumed = false;

        JobHandle orchestrator =
            scope.job_system
                .schedule([&] {
                    PendingBatch batch = scope.job_system.schedule_batch();
                    batch.add([&] { std::this_thread::sleep_for(std::chrono::milliseconds(10)); });

                    JobHandle batch_handle = batch.submit();

                    JobHandle waiter = scope.job_system
                                            .schedule([&] {
                                                waiter_started.store(true, std::memory_order_release);
                                                scope.job_system.wait_for(batch_handle);
                                                waiter_resumed.store(true, std::memory_order_release);
                                            })
                                            .submit();

                    while (!waiter_started.load(std::memory_order_acquire))
                    {
                        std::this_thread::yield();
                    }

                    scope.job_system.wait_for(waiter);
                })
                .submit();

        REQUIRE(scope.job_system.wait_for_blocking(orchestrator));
        REQUIRE(waiter_resumed.load(std::memory_order_acquire));
    }
}
