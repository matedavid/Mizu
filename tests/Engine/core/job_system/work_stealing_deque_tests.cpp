#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/job_system/work_stealing_deque.h"

using namespace Mizu;

struct ConcurrentCollector
{
    void push(int32_t value)
    {
        std::scoped_lock lock(mutex);
        values.push_back(value);
    }

    [[nodiscard]] std::vector<int32_t> snapshot() const
    {
        std::scoped_lock lock(mutex);
        return values;
    }

    mutable std::mutex mutex;
    std::vector<int32_t> values;
};

void wait_for_start(const std::atomic<bool>& start)
{
    while (!start.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

TEST_CASE("WorkStealingDeque basic operations behave correctly", "[WorkStealingDeque]")
{
    SECTION("pop and steal fail on an empty deque")
    {
        WorkStealingDeque<int32_t, 4> deque;

        int32_t popped = -1;
        int32_t stolen = -1;

        REQUIRE_FALSE(deque.pop(popped));
        REQUIRE_FALSE(deque.steal(stolen));
        REQUIRE(popped == -1);
        REQUIRE(stolen == -1);
    }

    SECTION("owner pops values in LIFO order")
    {
        WorkStealingDeque<int32_t, 4> deque;
        REQUIRE(deque.push(1));
        REQUIRE(deque.push(2));
        REQUIRE(deque.push(3));

        int32_t value = 0;

        REQUIRE(deque.pop(value));
        REQUIRE(value == 3);
        REQUIRE(deque.pop(value));
        REQUIRE(value == 2);
        REQUIRE(deque.pop(value));
        REQUIRE(value == 1);
        REQUIRE_FALSE(deque.pop(value));
    }

    SECTION("thief steals values in FIFO order")
    {
        WorkStealingDeque<int32_t, 4> deque;
        REQUIRE(deque.push(1));
        REQUIRE(deque.push(2));
        REQUIRE(deque.push(3));

        int32_t value = 0;

        REQUIRE(deque.steal(value));
        REQUIRE(value == 1);
        REQUIRE(deque.steal(value));
        REQUIRE(value == 2);
        REQUIRE(deque.steal(value));
        REQUIRE(value == 3);
        REQUIRE_FALSE(deque.steal(value));
    }

    SECTION("mixed owner and thief operations preserve remaining values")
    {
        WorkStealingDeque<int32_t, 4> deque;
        REQUIRE(deque.push(1));
        REQUIRE(deque.push(2));
        REQUIRE(deque.push(3));

        int32_t value = 0;

        REQUIRE(deque.steal(value));
        REQUIRE(value == 1);

        REQUIRE(deque.pop(value));
        REQUIRE(value == 3);

        REQUIRE(deque.pop(value));
        REQUIRE(value == 2);

        REQUIRE_FALSE(deque.pop(value));
        REQUIRE_FALSE(deque.steal(value));
    }

    SECTION("deque can be drained and reused")
    {
        WorkStealingDeque<int32_t, 3> deque;
        REQUIRE(deque.push(10));
        REQUIRE(deque.push(11));
        REQUIRE(deque.push(12));

        int32_t value = 0;

        REQUIRE(deque.pop(value));
        REQUIRE(value == 12);
        REQUIRE(deque.steal(value));
        REQUIRE(value == 10);
        REQUIRE(deque.pop(value));
        REQUIRE(value == 11);
        REQUIRE_FALSE(deque.pop(value));

        REQUIRE(deque.push(20));
        REQUIRE(deque.push(21));

        REQUIRE(deque.steal(value));
        REQUIRE(value == 20);
        REQUIRE(deque.pop(value));
        REQUIRE(value == 21);
        REQUIRE_FALSE(deque.steal(value));
    }
}

TEST_CASE("WorkStealingDeque handles exact capacity and full drain", "[WorkStealingDeque]")
{
    WorkStealingDeque<int32_t, 3> deque;
    REQUIRE(deque.push(1));
    REQUIRE(deque.push(2));
    REQUIRE(deque.push(3));

    int32_t value = 0;

    REQUIRE(deque.steal(value));
    REQUIRE(value == 1);
    REQUIRE(deque.pop(value));
    REQUIRE(value == 3);
    REQUIRE(deque.pop(value));
    REQUIRE(value == 2);
    REQUIRE_FALSE(deque.pop(value));
    REQUIRE_FALSE(deque.steal(value));
}

TEST_CASE("WorkStealingDeque push returns false when the deque is full", "[WorkStealingDeque]")
{
    WorkStealingDeque<int32_t, 3> deque;

    REQUIRE(deque.push(1));
    REQUIRE(deque.push(2));
    REQUIRE(deque.push(3));
    REQUIRE_FALSE(deque.push(4));

    int32_t value = -1;
    REQUIRE(deque.steal(value));
    REQUIRE(value == 1);
    REQUIRE(deque.pop(value));
    REQUIRE(value == 3);
    REQUIRE(deque.pop(value));
    REQUIRE(value == 2);
    REQUIRE_FALSE(deque.pop(value));
}

TEST_CASE("WorkStealingDeque reuses ring buffer slots across many wraparound cycles", "[WorkStealingDeque]")
{
    constexpr int32_t NumCycles = 128;

    WorkStealingDeque<int32_t, 3> deque;

    for (int32_t cycle = 0; cycle < NumCycles; ++cycle)
    {
        const int32_t base_value = cycle * 4;

        REQUIRE(deque.push(base_value + 0));
        REQUIRE(deque.push(base_value + 1));
        REQUIRE(deque.push(base_value + 2));

        int32_t value = -1;

        REQUIRE(deque.steal(value));
        REQUIRE(value == base_value + 0);

        REQUIRE(deque.push(base_value + 3));

        REQUIRE(deque.pop(value));
        REQUIRE(value == base_value + 3);
        REQUIRE(deque.pop(value));
        REQUIRE(value == base_value + 2);
        REQUIRE(deque.pop(value));
        REQUIRE(value == base_value + 1);

        REQUIRE_FALSE(deque.pop(value));
        REQUIRE_FALSE(deque.steal(value));
    }
}

TEST_CASE("WorkStealingDeque concurrent steals consume distinct front items", "[WorkStealingDeque]")
{
    WorkStealingDeque<int32_t, 4> deque;
    REQUIRE(deque.push(10));
    REQUIRE(deque.push(11));

    std::atomic<bool> start = false;

    std::array<bool, 2> steal_success{false, false};
    std::array<int32_t, 2> stolen_values{-1, -1};

    std::array<std::thread, 2> thieves;
    for (size_t index = 0; index < thieves.size(); ++index)
    {
        thieves[index] = std::thread([&, index] {
            wait_for_start(start);
            steal_success[index] = deque.steal(stolen_values[index]);
        });
    }

    start.store(true, std::memory_order_release);

    for (std::thread& thief : thieves)
    {
        thief.join();
    }

    REQUIRE(steal_success[0]);
    REQUIRE(steal_success[1]);
    REQUIRE(stolen_values[0] != stolen_values[1]);

    std::unordered_set<int32_t> unique_values(stolen_values.begin(), stolen_values.end());
    REQUIRE(unique_values.size() == 2);
    REQUIRE(unique_values.find(10) != unique_values.end());
    REQUIRE(unique_values.find(11) != unique_values.end());

    int32_t value = -1;
    REQUIRE_FALSE(deque.pop(value));
    REQUIRE_FALSE(deque.steal(value));
}

TEST_CASE("WorkStealingDeque last-item owner-thief races consume the value once", "[WorkStealingDeque]")
{
    constexpr size_t NumIterations = 512;

    size_t owner_wins = 0;
    size_t thief_wins = 0;

    for (size_t iteration = 0; iteration < NumIterations; ++iteration)
    {
        WorkStealingDeque<int32_t, 4> deque;
        REQUIRE(deque.push(static_cast<int32_t>(iteration)));

        std::atomic<bool> start = false;

        bool pop_success = false;
        bool steal_success = false;
        int32_t popped_value = -1;
        int32_t stolen_value = -1;

        std::thread owner([&] {
            wait_for_start(start);
            pop_success = deque.pop(popped_value);
        });

        std::thread thief([&] {
            wait_for_start(start);
            steal_success = deque.steal(stolen_value);
        });

        start.store(true, std::memory_order_release);

        owner.join();
        thief.join();

        REQUIRE((pop_success != steal_success));

        if (pop_success)
        {
            REQUIRE(popped_value == static_cast<int32_t>(iteration));
            owner_wins += 1;
        }

        if (steal_success)
        {
            REQUIRE(stolen_value == static_cast<int32_t>(iteration));
            thief_wins += 1;
        }

        int32_t value = -1;
        REQUIRE_FALSE(deque.pop(value));
        REQUIRE_FALSE(deque.steal(value));
    }

    REQUIRE(owner_wins + thief_wins == NumIterations);
}

TEST_CASE("WorkStealingDeque last-item thief-thief races consume the value once", "[WorkStealingDeque]")
{
    constexpr size_t NumIterations = 512;

    size_t first_thief_wins = 0;
    size_t second_thief_wins = 0;

    for (size_t iteration = 0; iteration < NumIterations; ++iteration)
    {
        WorkStealingDeque<int32_t, 4> deque;
        REQUIRE(deque.push(static_cast<int32_t>(iteration)));

        std::atomic<bool> start = false;

        std::array<bool, 2> steal_success{false, false};
        std::array<int32_t, 2> stolen_values{-1, -1};

        std::array<std::thread, 2> thieves;
        for (size_t index = 0; index < thieves.size(); ++index)
        {
            thieves[index] = std::thread([&, index] {
                wait_for_start(start);
                steal_success[index] = deque.steal(stolen_values[index]);
            });
        }

        start.store(true, std::memory_order_release);

        for (std::thread& thief : thieves)
        {
            thief.join();
        }

        REQUIRE(steal_success[0] != steal_success[1]);

        if (steal_success[0])
        {
            REQUIRE(stolen_values[0] == static_cast<int32_t>(iteration));
            first_thief_wins += 1;
        }

        if (steal_success[1])
        {
            REQUIRE(stolen_values[1] == static_cast<int32_t>(iteration));
            second_thief_wins += 1;
        }

        int32_t value = -1;
        REQUIRE_FALSE(deque.pop(value));
        REQUIRE_FALSE(deque.steal(value));
    }

    REQUIRE(first_thief_wins + second_thief_wins == NumIterations);
}

TEST_CASE("WorkStealingDeque concurrent push and steal preserves uniqueness", "[WorkStealingDeque]")
{
    constexpr int32_t NumItems = 256;
    constexpr size_t NumThieves = 3;

    WorkStealingDeque<int32_t, static_cast<size_t>(NumItems)> deque;

    std::atomic<bool> producer_done = false;
    std::atomic<int32_t> consumed_count = 0;
    ConcurrentCollector collector;

    std::thread producer([&] {
        for (int32_t value = 0; value < NumItems; ++value)
        {
            REQUIRE(deque.push(value));
        }

        producer_done.store(true, std::memory_order_release);
    });

    std::array<std::thread, NumThieves> thieves;
    for (std::thread& thief : thieves)
    {
        thief = std::thread([&] {
            while (true)
            {
                int32_t value = -1;
                if (deque.steal(value))
                {
                    collector.push(value);
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }

                if (producer_done.load(std::memory_order_acquire)
                    && consumed_count.load(std::memory_order_relaxed) == NumItems)
                {
                    break;
                }

                std::this_thread::yield();
            }
        });
    }

    producer.join();
    for (std::thread& thief : thieves)
    {
        thief.join();
    }

    const std::vector<int32_t> values = collector.snapshot();
    REQUIRE(values.size() == static_cast<size_t>(NumItems));

    std::unordered_set<int32_t> unique_values(values.begin(), values.end());
    REQUIRE(unique_values.size() == values.size());

    for (int32_t expected = 0; expected < NumItems; ++expected)
    {
        REQUIRE(unique_values.find(expected) != unique_values.end());
    }
}
