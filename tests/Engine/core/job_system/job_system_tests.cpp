#include <catch2/catch_all.hpp>

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
