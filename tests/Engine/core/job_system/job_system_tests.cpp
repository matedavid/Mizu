#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <cstdint>

#include "core/job_system/job_system.h"

using namespace Mizu;

struct JobSystemScope
{
    JobSystem2 job_system;
    bool initialized = false;

    ~JobSystemScope()
    {
        if (initialized)
        {
            job_system.wait_workers_dead();
        }
    }
};

TEST_CASE("JobSystem2 executes a single scheduled job", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> executed = false;

    JobHandle2 handle =
        scope.job_system.schedule([&executed] { executed.store(true, std::memory_order_release); }).submit();

    REQUIRE(scope.job_system.wait_for_blocking(handle));
    REQUIRE(executed.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem2 executes dependent jobs in order", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> first_order = 0;
    std::atomic<int32_t> second_order = 0;

    JobHandle2 first =
        scope.job_system
            .schedule(
                [&] { first_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release); })
            .submit();

    JobHandle2 second =
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

TEST_CASE("JobSystem2 executes a job after multiple dependencies complete", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> first_order = 0;
    std::atomic<int32_t> second_order = 0;
    std::atomic<int32_t> final_order = 0;

    JobHandle2 first =
        scope.job_system
            .schedule(
                [&] { first_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release); })
            .submit();

    JobHandle2 second =
        scope.job_system
            .schedule([&] {
                second_order.store(step.fetch_add(1, std::memory_order_acq_rel) + 1, std::memory_order_release);
            })
            .submit();

    JobHandle2 final_job =
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

TEST_CASE("JobSystem2 handles already completed dependencies", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    std::atomic<bool> dependency_done = false;
    std::atomic<bool> dependent_done = false;

    JobHandle2 dependency =
        scope.job_system.schedule([&] { dependency_done.store(true, std::memory_order_release); }).submit();

    REQUIRE(scope.job_system.wait_for_blocking(dependency));

    JobHandle2 dependent = scope.job_system.schedule([&] { dependent_done.store(true, std::memory_order_release); })
                               .depends_on(dependency)
                               .submit();

    REQUIRE(scope.job_system.wait_for_blocking(dependent));
    REQUIRE(dependency_done.load(std::memory_order_acquire));
    REQUIRE(dependent_done.load(std::memory_order_acquire));
}

TEST_CASE("JobSystem2 returns false for an invalid handle", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(2, false));
    scope.initialized = true;

    REQUIRE_FALSE(scope.job_system.wait_for_blocking(JobHandle2{}));
}

TEST_CASE("JobSystem2 batch submission executes all jobs", "[JobSystem2]")
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

    JobHandle2 batch_handle = batch.submit();
    REQUIRE(scope.job_system.wait_for_blocking(batch_handle));

    for (const std::atomic<int32_t>& value : values)
    {
        REQUIRE(value.load(std::memory_order_acquire) == 1);
    }
}

TEST_CASE("JobSystem2 batch submission respects upstream dependencies", "[JobSystem2]")
{
    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    constexpr size_t NumJobs = 8;
    std::atomic<bool> setup_done = false;
    std::atomic<int32_t> violations = 0;
    std::atomic<int32_t> executed = 0;

    JobHandle2 setup = scope.job_system.schedule([&] { setup_done.store(true, std::memory_order_release); }).submit();

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

    JobHandle2 batch_handle = batch.depends_on(setup).submit();
    REQUIRE(scope.job_system.wait_for_blocking(batch_handle));
    REQUIRE(violations.load(std::memory_order_acquire) == 0);
    REQUIRE(executed.load(std::memory_order_acquire) == static_cast<int32_t>(NumJobs));
}

TEST_CASE("JobSystem2 can schedule a job after a batch completes", "[JobSystem2]")
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

        JobHandle2 batch_handle = batch.submit();
        JobHandle2 finisher =
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

TEST_CASE("JobSystem2 can schedule a batch after another batch completes", "[JobSystem2]")
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
    JobHandle2 first_handle = first_batch.submit();

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

    JobHandle2 second_handle = second_batch.depends_on(first_handle).submit();
    REQUIRE(scope.job_system.wait_for_blocking(second_handle));
    REQUIRE(first_batch_count.load(std::memory_order_acquire) == NumJobs);
    REQUIRE(second_batch_count.load(std::memory_order_acquire) == NumJobs);
    REQUIRE(ordering_violations.load(std::memory_order_acquire) == 0);
}
