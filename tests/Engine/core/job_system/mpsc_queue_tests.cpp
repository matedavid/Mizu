#include <catch2/catch_all.hpp>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "core/job_system/mpsc_queue.h"

using namespace Mizu;

template <typename T>
struct ConcurrentCollector
{
    void push(T value)
    {
        std::scoped_lock lock(mutex);
        values.push_back(value);
    }

    std::vector<T> snapshot() const
    {
        std::scoped_lock lock(mutex);
        return values;
    }

    mutable std::mutex mutex;
    std::vector<T> values;
};

void wait_for_mpsc_start(const std::atomic<bool>& start)
{
    while (!start.load(std::memory_order_acquire))
    {
        std::this_thread::yield();
    }
}

template <size_t NumProducers>
struct SinglePushRoundResult
{
    std::array<bool, NumProducers> push_success{};
    std::vector<size_t> popped_values;
};

template <size_t Capacity, size_t NumProducers>
SinglePushRoundResult<NumProducers> run_single_push_contention_round()
{
    MpscQueue<size_t, Capacity> queue;

    std::atomic<bool> start = false;
    std::array<std::thread, NumProducers> producers;

    SinglePushRoundResult<NumProducers> result;

    for (size_t producer_index = 0; producer_index < NumProducers; ++producer_index)
    {
        producers[producer_index] = std::thread([&, producer_index] {
            wait_for_mpsc_start(start);
            result.push_success[producer_index] = queue.push(producer_index);
        });
    }

    start.store(true, std::memory_order_release);

    for (std::thread& producer : producers)
    {
        producer.join();
    }

    result.popped_values.reserve(NumProducers);

    size_t value = 0;
    while (queue.pop(value))
    {
        result.popped_values.push_back(value);
    }

    return result;
}

TEST_CASE("MpscQueue basic operations behave correctly", "[MpscQueue]")
{
    SECTION("pop fails on an empty queue")
    {
        MpscQueue<int32_t, 4> queue;

        int32_t value = -1;

        REQUIRE_FALSE(queue.pop(value));
        REQUIRE(value == -1);
    }

    SECTION("push and pop handle a single value")
    {
        MpscQueue<int32_t, 4> queue;

        REQUIRE(queue.push(42));

        int32_t value = -1;
        REQUIRE(queue.pop(value));
        REQUIRE(value == 42);
        REQUIRE_FALSE(queue.pop(value));
    }

    SECTION("pop returns values in FIFO order")
    {
        MpscQueue<int32_t, 4> queue;
        REQUIRE(queue.push(1));
        REQUIRE(queue.push(2));
        REQUIRE(queue.push(3));

        int32_t value = -1;

        REQUIRE(queue.pop(value));
        REQUIRE(value == 1);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 2);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 3);
        REQUIRE_FALSE(queue.pop(value));
    }

    SECTION("queue handles exact capacity and can be reused after draining")
    {
        MpscQueue<int32_t, 4> queue;
        REQUIRE(queue.push(10));
        REQUIRE(queue.push(11));
        REQUIRE(queue.push(12));
        REQUIRE(queue.push(13));
        REQUIRE_FALSE(queue.push(14));

        int32_t value = -1;
        REQUIRE(queue.pop(value));
        REQUIRE(value == 10);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 11);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 12);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 13);
        REQUIRE_FALSE(queue.pop(value));

        REQUIRE(queue.push(20));
        REQUIRE(queue.push(21));

        REQUIRE(queue.pop(value));
        REQUIRE(value == 20);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 21);
        REQUIRE_FALSE(queue.pop(value));
    }

    SECTION("queue semantics stay correct across repeated small fills")
    {
        MpscQueue<int32_t, 4> queue;
        REQUIRE(queue.push(100));
        REQUIRE(queue.push(101));

        int32_t value = -1;
        REQUIRE(queue.pop(value));
        REQUIRE(value == 100);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 101);

        REQUIRE(queue.push(102));
        REQUIRE(queue.push(103));
        REQUIRE(queue.pop(value));
        REQUIRE(value == 102);
        REQUIRE(queue.pop(value));
        REQUIRE(value == 103);
        REQUIRE_FALSE(queue.pop(value));
    }
}

TEST_CASE("MpscQueue reuses ring buffer slots across wraparound cycles", "[MpscQueue]")
{
    constexpr int32_t NumCycles = 128;

    MpscQueue<int32_t, 4> queue;

    for (int32_t cycle = 0; cycle < NumCycles; ++cycle)
    {
        const int32_t base_value = cycle * 6;

        REQUIRE(queue.push(base_value + 0));
        REQUIRE(queue.push(base_value + 1));
        REQUIRE(queue.push(base_value + 2));
        REQUIRE(queue.push(base_value + 3));

        int32_t value = -1;

        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 0);
        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 1);

        REQUIRE(queue.push(base_value + 4));
        REQUIRE(queue.push(base_value + 5));

        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 2);
        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 3);
        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 4);
        REQUIRE(queue.pop(value));
        REQUIRE(value == base_value + 5);
        REQUIRE_FALSE(queue.pop(value));
    }
}

TEST_CASE("MpscQueue concurrent producers preserve uniqueness", "[MpscQueue]")
{
    constexpr size_t NumProducers = 4;
    constexpr size_t ItemsPerProducer = 32;
    constexpr size_t NumItems = NumProducers * ItemsPerProducer;

    MpscQueue<size_t, NumItems> queue;

    std::atomic<bool> start = false;

    std::array<std::thread, NumProducers> producers;
    for (size_t producer_index = 0; producer_index < NumProducers; ++producer_index)
    {
        producers[producer_index] = std::thread([&, producer_index] {
            wait_for_mpsc_start(start);

            const size_t base_value = producer_index * ItemsPerProducer;
            for (size_t offset = 0; offset < ItemsPerProducer; ++offset)
            {
                const size_t value = base_value + offset;
                while (!queue.push(value))
                {
                    std::this_thread::yield();
                }
            }
        });
    }

    start.store(true, std::memory_order_release);

    for (std::thread& producer : producers)
    {
        producer.join();
    }

    std::vector<size_t> values;
    values.reserve(NumItems);

    size_t value = 0;
    while (queue.pop(value))
    {
        values.push_back(value);
    }

    REQUIRE(values.size() == NumItems);

    std::unordered_set<size_t> unique_values(values.begin(), values.end());
    REQUIRE(unique_values.size() == values.size());

    for (size_t expected = 0; expected < NumItems; ++expected)
    {
        REQUIRE(unique_values.find(expected) != unique_values.end());
    }
}

TEST_CASE("MpscQueue bounded contention matches successful pushes", "[MpscQueue]")
{
    constexpr size_t NumProducers = 32;
    constexpr size_t Capacity = 16;

    MpscQueue<size_t, Capacity> queue;

    std::atomic<bool> start = false;
    std::array<bool, NumProducers> push_success{};
    std::array<std::thread, NumProducers> producers;

    for (size_t producer_index = 0; producer_index < NumProducers; ++producer_index)
    {
        producers[producer_index] = std::thread([&, producer_index] {
            wait_for_mpsc_start(start);
            push_success[producer_index] = queue.push(producer_index);
        });
    }

    start.store(true, std::memory_order_release);

    for (std::thread& producer : producers)
    {
        producer.join();
    }

    size_t success_count = 0;
    for (const bool success : push_success)
    {
        success_count += success ? 1u : 0u;
    }

    std::vector<size_t> values;
    values.reserve(success_count);

    size_t value = 0;
    while (queue.pop(value))
    {
        values.push_back(value);
    }

    REQUIRE(values.size() == success_count);
    REQUIRE(values.size() <= Capacity);

    std::unordered_set<size_t> unique_values(values.begin(), values.end());
    REQUIRE(unique_values.size() == values.size());

    for (const size_t pushed_value : values)
    {
        REQUIRE(push_success[pushed_value]);
    }
}

TEST_CASE("MpscQueue concurrent pushes succeed while capacity remains available", "[MpscQueue]")
{
    constexpr size_t NumProducers = 16;
    constexpr size_t Capacity = 64;

    const SinglePushRoundResult<NumProducers> result = run_single_push_contention_round<Capacity, NumProducers>();

    size_t success_count = 0;
    for (const bool success : result.push_success)
    {
        success_count += success ? 1u : 0u;
    }

    REQUIRE(success_count == NumProducers);
    REQUIRE(result.popped_values.size() == NumProducers);

    std::unordered_set<size_t> unique_values(result.popped_values.begin(), result.popped_values.end());
    REQUIRE(unique_values.size() == result.popped_values.size());

    for (size_t expected = 0; expected < NumProducers; ++expected)
    {
        REQUIRE(unique_values.find(expected) != unique_values.end());
    }
}

TEST_CASE("MpscQueue sustained contention succeeds before becoming full", "[MpscQueue]")
{
    constexpr size_t NumIterations = 128;
    constexpr size_t NumProducers = 16;
    constexpr size_t Capacity = 64;

    for (size_t iteration = 0; iteration < NumIterations; ++iteration)
    {
        const SinglePushRoundResult<NumProducers> result = run_single_push_contention_round<Capacity, NumProducers>();

        size_t success_count = 0;
        for (const bool success : result.push_success)
        {
            success_count += success ? 1u : 0u;
        }

        REQUIRE(success_count == NumProducers);
        REQUIRE(result.popped_values.size() == NumProducers);

        std::unordered_set<size_t> unique_values(result.popped_values.begin(), result.popped_values.end());
        REQUIRE(unique_values.size() == result.popped_values.size());

        for (size_t expected = 0; expected < NumProducers; ++expected)
        {
            REQUIRE(unique_values.find(expected) != unique_values.end());
        }
    }
}

TEST_CASE("MpscQueue concurrent producer and consumer preserve all values", "[MpscQueue]")
{
    constexpr size_t NumProducers = 4;
    constexpr size_t ItemsPerProducer = 32;
    constexpr size_t NumItems = NumProducers * ItemsPerProducer;
    constexpr size_t Capacity = 32;

    MpscQueue<size_t, Capacity> queue;

    std::atomic<bool> start = false;
    std::atomic<size_t> consumed_count = 0;
    std::atomic<size_t> producers_finished = 0;
    ConcurrentCollector<size_t> collector;

    std::array<std::thread, NumProducers> producers;
    for (size_t producer_index = 0; producer_index < NumProducers; ++producer_index)
    {
        producers[producer_index] = std::thread([&, producer_index] {
            wait_for_mpsc_start(start);

            const size_t base_value = producer_index * ItemsPerProducer;
            for (size_t offset = 0; offset < ItemsPerProducer; ++offset)
            {
                const size_t value = base_value + offset;
                while (!queue.push(value))
                {
                    std::this_thread::yield();
                }
            }

            producers_finished.fetch_add(1, std::memory_order_release);
        });
    }

    std::thread consumer([&] {
        wait_for_mpsc_start(start);

        while (true)
        {
            size_t value = 0;
            if (queue.pop(value))
            {
                collector.push(value);
                consumed_count.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            if (producers_finished.load(std::memory_order_acquire) == NumProducers
                && consumed_count.load(std::memory_order_relaxed) == NumItems)
            {
                break;
            }

            std::this_thread::yield();
        }
    });

    start.store(true, std::memory_order_release);

    for (std::thread& producer : producers)
    {
        producer.join();
    }

    consumer.join();

    const std::vector<size_t> values = collector.snapshot();
    REQUIRE(values.size() == NumItems);

    std::unordered_set<size_t> unique_values(values.begin(), values.end());
    REQUIRE(unique_values.size() == values.size());

    for (size_t expected = 0; expected < NumItems; ++expected)
    {
        REQUIRE(unique_values.find(expected) != unique_values.end());
    }

    size_t value = 0;
    REQUIRE_FALSE(queue.pop(value));
}