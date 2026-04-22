#include <catch2/catch_all.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/job_system/job_system.h"

using namespace Mizu;

namespace
{

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

} // namespace

TEST_CASE("JobSystem2 reuses completion records across more than pool capacity jobs", "[JobSystem2][stress]")
{
    constexpr int32_t NumJobs = 3000;

    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;

    for (int32_t index = 0; index < NumJobs; ++index)
    {
        JobHandle2 handle = scope.job_system.schedule([&] {
                                   executed.fetch_add(1, std::memory_order_acq_rel);
                               }).submit();

        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumJobs);
}

TEST_CASE("JobSystem2 reuses completion records across many batches", "[JobSystem2][stress]")
{
    constexpr int32_t NumBatches = 512;
    constexpr int32_t JobsPerBatch = 4;

    JobSystemScope scope;
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

        JobHandle2 handle = batch.submit();
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumBatches * JobsPerBatch);
}

TEST_CASE("JobSystem2 executes a large fan-out of independent jobs", "[JobSystem2][stress]")
{
    constexpr int32_t NumJobs = 256;

    JobSystemScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;
    std::vector<JobHandle2> handles;
    handles.reserve(static_cast<size_t>(NumJobs));

    for (int32_t index = 0; index < NumJobs; ++index)
    {
        handles.push_back(scope.job_system.schedule([&] {
            executed.fetch_add(1, std::memory_order_acq_rel);
        }).submit());
    }

    for (const JobHandle2& handle : handles)
    {
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumJobs);
}

TEST_CASE("JobSystem2 executes a deep dependency chain", "[JobSystem2][stress]")
{
    constexpr int32_t ChainLength = 64;

    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    std::atomic<int32_t> step = 0;
    std::atomic<int32_t> final_order = 0;
    std::vector<JobHandle2> handles;
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

TEST_CASE("JobSystem2 executes multiple batches concurrently", "[JobSystem2][stress]")
{
    constexpr int32_t NumBatches = 16;
    constexpr int32_t JobsPerBatch = 8;

    JobSystemScope scope;
    REQUIRE(scope.job_system.init(4, false));
    scope.initialized = true;

    std::atomic<int32_t> executed = 0;
    std::vector<JobHandle2> handles;
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

    for (const JobHandle2& handle : handles)
    {
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE(executed.load(std::memory_order_acquire) == NumBatches * JobsPerBatch);
}

TEST_CASE("JobSystem2 rejects stale handles after pool reuse", "[JobSystem2][stress]")
{
    constexpr int32_t NumReuseIterations = 3000;

    JobSystemScope scope;
    REQUIRE(scope.job_system.init(3, false));
    scope.initialized = true;

    JobHandle2 stale_handle{};
    {
        JobHandle2 handle = scope.job_system.schedule([] {}).submit();
        stale_handle.completion_index = handle.completion_index;
        stale_handle.generation = handle.generation;
        stale_handle.owner = handle.owner;

        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    for (int32_t index = 0; index < NumReuseIterations; ++index)
    {
        JobHandle2 handle = scope.job_system.schedule([] {}).submit();
        REQUIRE(scope.job_system.wait_for_blocking(handle));
    }

    REQUIRE_FALSE(scope.job_system.wait_for_blocking(stale_handle));
}
