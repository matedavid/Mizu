#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "core/job_system/job_system.h"

using namespace Mizu;

struct JobSystemScope
{
    JobSystem job_system;
    bool initialized = false;

    ~JobSystemScope()
    {
        if (initialized)
        {
            job_system.wait_workers_dead();
        }
    }
};

TEST_CASE("JobSystem executes a single scheduled job", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> executed = false;

    JobHandle handle =
        scope.job_system.schedule([&executed] { executed.store(true, std::memory_order_release); }).submit();

    REQUIRE(scope.job_system.wait_for_blocking(handle));
    REQUIRE(executed.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem executes dependent jobs in order", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> first_order = 0;
    std::atomic<int32_t> second_order = 0;

    JobHandle first =
        scope.job_system
            .schedule(
                [&] { first_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release); })
            .submit();

    JobHandle second =
        scope.job_system
            .schedule([&] {
                second_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release);
            })
            .depends_on(first)
            .submit();

    REQUIRE(scope.job_system.wait_for_blocking(second));
    REQUIRE(first_order.load(std::memory_order_acquire) == 1);
    REQUIRE(second_order.load(std::memory_order_acquire) == 2);
}

TEST_CASE("JobSystem executes a job after multiple dependencies complete", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> first_order = 0;
    std::atomic<int32_t> second_order = 0;
    std::atomic<int32_t> final_order = 0;

    JobHandle first =
        scope.job_system
            .schedule(
                [&] { first_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release); })
            .submit();

    JobHandle second =
        scope.job_system
            .schedule([&] {
                second_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release);
            })
            .submit();

    JobHandle final_job =
        scope.job_system
            .schedule(
                [&] { final_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release); })
            .depends_on(first)
            .depends_on(second)
            .submit();

    REQUIRE(scope.job_system.wait_for_blocking(final_job));

    const int32_t first_value = first_order.load(std::memory_order_acquire);
    const int32_t second_value = second_order.load(std::memory_order_acquire);
    const int32_t final_value = final_order.load(std::memory_order_acquire);

    REQUIRE(first_value > 0);
    REQUIRE(second_value > 0);
    REQUIRE(final_value == 3);
    REQUIRE(first_value < final_value);
    REQUIRE(second_value < final_value);
}

TEST_CASE("JobSystem handles already completed dependencies", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> dependency_done = false;
    std::atomic<bool> dependent_done = false;

    JobHandle dependency =
        scope.job_system.schedule([&] { dependency_done.store(true, std::memory_order_release); }).submit();

    REQUIRE(scope.job_system.wait_for_blocking(dependency));

    JobHandle dependent = scope.job_system.schedule([&] { dependent_done.store(true, std::memory_order_release); })
                               .depends_on(dependency)
                               .submit();

    REQUIRE(scope.job_system.wait_for_blocking(dependent));
    REQUIRE(dependency_done.load(std::memory_order_acquire));
    REQUIRE(dependent_done.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem returns false for an invalid handle", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    REQUIRE_FALSE(scope.job_system.wait_for_blocking(JobHandle{}));
}

TEST_CASE("JobSystem handles immediate shutdown after init without scheduling work", "[JobSystem]")
{
    // Regression test for late-start detached thread race: ensure that when workers
    // are spawned but not yet observed as alive, the system can cleanly shutdown.
    // This exercises the pre-spawn increment of m_num_workers_alive that catches
    // the window before a worker thread observes itself as alive.
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    // No jobs scheduled; threads are yielding waiting for m_is_enabled to become false.
    // The destructor calls wait_workers_dead() which sets m_is_enabled to false
    // and waits for all workers to decrement the alive count and exit.
    // This must not hang even if some threads hadn't started yet.
}

TEST_CASE("JobSystem batch submission executes all jobs", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    constexpr size_t NumJobs = 8;
    std::array<std::atomic<int32_t>, NumJobs> values{};
    for (std::atomic<int32_t>& value : values)
    {
        value.store(0, std::memory_order_relaxed);
    }

    PendingBatch batch = scope.job_system.schedule_batch();
    for (size_t index = 0; index < NumJobs; ++index)
    {
        batch.add([&values, index] { values[index].store(1, std::memory_order_release); });
    }

    JobHandle batch_handle = batch.submit();
    REQUIRE(scope.job_system.wait_for_blocking(batch_handle));

    for (const std::atomic<int32_t>& value : values)
    {
        REQUIRE(value.load(std::memory_order_acquire) == 1);
    }
}

TEST_CASE("JobSystem batch submission respects upstream dependencies", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    constexpr size_t NumJobs = 8;
    std::atomic<bool> setup_done = false;
    std::atomic<int32_t> violations = 0;
    std::atomic<int32_t> executed = 0;

    JobHandle setup = scope.job_system.schedule([&] { setup_done.store(true, std::memory_order_release); }).submit();

    PendingBatch batch = scope.job_system.schedule_batch();
    for (size_t index = 0; index < NumJobs; ++index)
    {
        batch.add([&] {
            if (!setup_done.load(std::memory_order_acquire))
            {
                violations.fetch_add(1, std::memory_order_acq_rel);
            }

            executed.fetch_add(1, std::memory_order_acq_rel);
        });
    }

    JobHandle batch_handle = batch.depends_on(setup).submit();
    REQUIRE(scope.job_system.wait_for_blocking(batch_handle));
    REQUIRE(violations.load(std::memory_order_acquire) == 0);
    REQUIRE(executed.load(std::memory_order_acquire) == static_cast<int32_t>(NumJobs));
}

TEST_CASE("JobSystem can schedule a job after a batch completes", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    constexpr int32_t NumJobs = 8;
    constexpr int32_t Iterations = 200;

    for (int32_t iteration = 0; iteration < Iterations; ++iteration)
    {
        std::atomic<int32_t> batch_count = 0;
        std::atomic<int32_t> finisher_observed = 0;

        PendingBatch batch = scope.job_system.schedule_batch();
        for (int32_t index = 0; index < NumJobs; ++index)
        {
            batch.add([&] { batch_count.fetch_add(1, std::memory_order_acq_rel); });
        }

        JobHandle batch_handle = batch.submit();
        JobHandle finisher =
            scope.job_system
                .schedule([&] {
                    finisher_observed.store(batch_count.load(std::memory_order_acquire), std::memory_order_release);
                })
                .depends_on(batch_handle)
                .submit();

        REQUIRE(scope.job_system.wait_for_blocking(finisher));
        REQUIRE(batch_count.load(std::memory_order_acquire) == NumJobs);
        REQUIRE(finisher_observed.load(std::memory_order_acquire) == NumJobs);
    }
}

TEST_CASE("JobSystem can schedule a batch after another batch completes", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(1, false));
    scope.initialized = true;

    constexpr int32_t NumJobs = 8;
    std::atomic<int32_t> first_batch_count = 0;
    std::atomic<int32_t> second_batch_count = 0;
    std::atomic<int32_t> ordering_violations = 0;

    PendingBatch first_batch = scope.job_system.schedule_batch();
    for (int32_t index = 0; index < NumJobs; ++index)
    {
        first_batch.add([&] { first_batch_count.fetch_add(1, std::memory_order_acq_rel); });
    }
    JobHandle first_handle = first_batch.submit();

    PendingBatch second_batch = scope.job_system.schedule_batch();
    for (int32_t index = 0; index < NumJobs; ++index)
    {
        second_batch.add([&] {
            if (first_batch_count.load(std::memory_order_acquire) != NumJobs)
            {
                ordering_violations.fetch_add(1, std::memory_order_acq_rel);
            }

            second_batch_count.fetch_add(1, std::memory_order_acq_rel);
        });
    }

    JobHandle second_handle = second_batch.depends_on(first_handle).submit();
    REQUIRE(scope.job_system.wait_for_blocking(second_handle));
    REQUIRE(first_batch_count.load(std::memory_order_acquire) == NumJobs);
    REQUIRE(second_batch_count.load(std::memory_order_acquire) == NumJobs);
    REQUIRE(ordering_violations.load(std::memory_order_acquire) == 0);
}

TEST_CASE("JobSystem supports wait_for from inside a running job fiber", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> dependency_completed = false;
    std::atomic<bool> waiter_started = false;
    std::atomic<bool> waiter_resumed = false;
    std::atomic<bool> waiter_observed_dependency_done = false;

    JobHandle waiter =
        scope.job_system
            .schedule([&] {
                JobHandle dependency = scope.job_system
                                            .schedule([&] {
                                                dependency_completed.store(true, std::memory_order_release);
                                            })
                                            .submit();

                waiter_started.store(true, std::memory_order_release);
                scope.job_system.wait_for(dependency);
                waiter_observed_dependency_done.store(
                    dependency_completed.load(std::memory_order_acquire), std::memory_order_release);
                waiter_resumed.store(true, std::memory_order_release);
            })
            .submit();

    REQUIRE(scope.job_system.wait_for_blocking(waiter));
    REQUIRE(dependency_completed.load(std::memory_order_acquire));
    REQUIRE(waiter_started.load(std::memory_order_acquire));
    REQUIRE(waiter_resumed.load(std::memory_order_acquire));
    REQUIRE(waiter_observed_dependency_done.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem wait_for inside a job handles already completed dependency", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> dependency_completed = false;
    std::atomic<bool> waiter_executed = false;
    std::atomic<bool> waiter_observed_dependency_done = false;

    JobHandle waiter =
        scope.job_system
            .schedule([&] {
                JobHandle dependency = scope.job_system
                                            .schedule([&] {
                                                dependency_completed.store(true, std::memory_order_release);
                                            })
                                            .submit();

                scope.job_system.wait_for(dependency);
                scope.job_system.wait_for(dependency);
                waiter_observed_dependency_done.store(
                    dependency_completed.load(std::memory_order_acquire), std::memory_order_release);
                waiter_executed.store(true, std::memory_order_release);
            })
            .submit();

    REQUIRE(scope.job_system.wait_for_blocking(waiter));
    REQUIRE(waiter_executed.load(std::memory_order_acquire));
    REQUIRE(waiter_observed_dependency_done.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem supports nested wait_for calls across jobs", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(1, false));
    scope.initialized = true;

    std::atomic<int32_t> phase = 0;
    std::atomic<int32_t> middle_order = 0;
    std::atomic<int32_t> root_order = 0;

    JobHandle root =
        scope.job_system
            .schedule([&] {
                JobHandle middle = scope.job_system
                                        .schedule([&] {
                                            JobHandle leaf = scope.job_system
                                                                  .schedule([&] {
                                                                      phase.fetch_add(1, std::memory_order_acq_rel);
                                                                  })
                                                                  .submit();

                                            scope.job_system.wait_for(leaf);
                                            middle_order.store(
                                                phase.fetch_add(1, std::memory_order_acq_rel) + 1,
                                                std::memory_order_release);
                                        })
                                        .submit();

                scope.job_system.wait_for(middle);
                root_order.store(phase.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release);
            })
            .submit();

    REQUIRE(scope.job_system.wait_for_blocking(root));
    REQUIRE(middle_order.load(std::memory_order_acquire) == 2);
    REQUIRE(root_order.load(std::memory_order_acquire) == 3);
}

TEST_CASE("JobSystem resumes all jobs waiting on the same dependency", "[JobSystem]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    constexpr int32_t NumWaiters = 6;

    std::atomic<bool> dependency_can_finish = false;
    std::atomic<bool> dependency_completed = false;
    std::atomic<int32_t> waiters_started = 0;
    std::atomic<int32_t> waiters_resumed = 0;
    std::atomic<int32_t> ordering_violations = 0;

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
                waiter_handles.reserve(static_cast<size_t>(NumWaiters));

                for (int32_t index = 0; index < NumWaiters; ++index)
                {
                    waiter_handles.push_back(
                        scope.job_system
                            .schedule([&] {
                                waiters_started.fetch_add(1, std::memory_order_acq_rel);
                                scope.job_system.wait_for(dependency);

                                if (!dependency_completed.load(std::memory_order_acquire))
                                {
                                    ordering_violations.fetch_add(1, std::memory_order_acq_rel);
                                }

                                waiters_resumed.fetch_add(1, std::memory_order_acq_rel);
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
    REQUIRE(ordering_violations.load(std::memory_order_acquire) == 0);
}
