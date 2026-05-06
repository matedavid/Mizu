#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <thread>
#include <vector>

#include "core/job_system/intrusive_free_list.h"

using namespace Mizu;

struct TestRecord
{
    IntrusiveFreeListIndex pool_index = IntrusiveFreeListInvalidIndex;
    IntrusiveFreeListIndex next_free = IntrusiveFreeListInvalidIndex;
};

void wait_for_intrusive_free_list_start(const std::atomic<bool>& start)
{
    while (!start.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

TEST_CASE("IntrusiveFreeList exhausts and reuses all slots", "[IntrusiveFreeList]")
{
    constexpr size_t Capacity = 8;
    IntrusiveFreeList<TestRecord, Capacity> pool;

    std::array<bool, Capacity> seen{};
    for (size_t index = 0; index < Capacity; ++index)
    {
        const IntrusiveFreeListIndex allocated = pool.allocate();
        REQUIRE(allocated != IntrusiveFreeListInvalidIndex);
        REQUIRE(allocated < Capacity);
        REQUIRE_FALSE(seen[allocated]);
        seen[allocated] = true;
    }

    REQUIRE(pool.allocate() == IntrusiveFreeListInvalidIndex);

    for (IntrusiveFreeListIndex index = 0; index < Capacity; ++index)
    {
        pool.free(index);
    }

    seen.fill(false);
    for (size_t index = 0; index < Capacity; ++index)
    {
        const IntrusiveFreeListIndex allocated = pool.allocate();
        REQUIRE(allocated != IntrusiveFreeListInvalidIndex);
        REQUIRE(allocated < Capacity);
        REQUIRE_FALSE(seen[allocated]);
        seen[allocated] = true;
    }

    REQUIRE(pool.allocate() == IntrusiveFreeListInvalidIndex);
}

TEST_CASE("IntrusiveFreeList survives interleaved allocation cycles", "[IntrusiveFreeList]")
{
    constexpr size_t Capacity = 16;
    IntrusiveFreeList<TestRecord, Capacity> pool;
    std::array<bool, Capacity> live{};
    std::vector<IntrusiveFreeListIndex> allocated_indices;
    allocated_indices.reserve(4);

    for (size_t iteration = 0; iteration < 256; ++iteration)
    {
        allocated_indices.clear();
        for (size_t alloc_index = 0; alloc_index < 4; ++alloc_index)
        {
            const IntrusiveFreeListIndex allocated = pool.allocate();
            REQUIRE(allocated != IntrusiveFreeListInvalidIndex);
            REQUIRE_FALSE(live[allocated]);
            live[allocated] = true;
            allocated_indices.push_back(allocated);
        }

        pool.free(allocated_indices[1]);
        live[allocated_indices[1]] = false;
        pool.free(allocated_indices[3]);
        live[allocated_indices[3]] = false;

        const IntrusiveFreeListIndex first_reused = pool.allocate();
        REQUIRE(first_reused != IntrusiveFreeListInvalidIndex);
        REQUIRE_FALSE(live[first_reused]);
        live[first_reused] = true;

        const IntrusiveFreeListIndex second_reused = pool.allocate();
        REQUIRE(second_reused != IntrusiveFreeListInvalidIndex);
        REQUIRE_FALSE(live[second_reused]);
        live[second_reused] = true;

        for (IntrusiveFreeListIndex index = 0; index < Capacity; ++index)
        {
            if (live[index])
            {
                pool.free(index);
                live[index] = false;
            }
        }
    }
}

TEST_CASE("IntrusiveFreeList concurrent allocate and free keeps slots unique", "[IntrusiveFreeList]")
{
    constexpr size_t Capacity = 32;
    constexpr size_t NumThreads = 8;
    constexpr size_t IterationsPerThread = 512;

    IntrusiveFreeList<TestRecord, Capacity> pool;
    std::array<std::atomic<bool>, Capacity> live{};
    for (std::atomic<bool>& entry : live)
    {
        entry.store(false, std::memory_order_relaxed);
    }

    std::atomic<bool> start = false;
    std::atomic<int32_t> violations = 0;
    std::array<std::thread, NumThreads> threads;

    for (size_t thread_index = 0; thread_index < NumThreads; ++thread_index)
    {
        threads[thread_index] = std::thread([&] {
            wait_for_intrusive_free_list_start(start);

            for (size_t iteration = 0; iteration < IterationsPerThread; ++iteration)
            {
                IntrusiveFreeListIndex allocated = IntrusiveFreeListInvalidIndex;
                while (allocated == IntrusiveFreeListInvalidIndex)
                {
                    allocated = pool.allocate();
                    if (allocated == IntrusiveFreeListInvalidIndex)
                    {
                        std::this_thread::yield();
                    }
                }

                if (live[allocated].exchange(true, std::memory_order_acq_rel))
                {
                    violations.fetch_add(1, std::memory_order_acq_rel);
                }

                if (!live[allocated].exchange(false, std::memory_order_acq_rel))
                {
                    violations.fetch_add(1, std::memory_order_acq_rel);
                }

                pool.free(allocated);
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (std::thread& thread : threads)
    {
        thread.join();
    }

    REQUIRE(violations.load(std::memory_order_acquire) == 0);
}
