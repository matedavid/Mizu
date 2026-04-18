#pragma once

#include <atomic>
#include <vector>

namespace Mizu
{

template <typename T, size_t Capacity>
class WorkStealingDeque
{
    static_assert(Capacity > 0, "Can't create WorkStealingDeque with Capacity == 0");
    static_assert(std::is_trivially_destructible_v<T>, "T must be trivially destructible");

    static constexpr int64_t IntCapacity = static_cast<int64_t>(Capacity);

  public:
    WorkStealingDeque()
    {
        m_queue.resize(Capacity);

        m_top.store(0);
        m_bottom.store(0);
    }

    bool push(T value)
    {
        const int64_t bottom = m_bottom.load(std::memory_order_relaxed);
        const int64_t top = m_top.load(std::memory_order_acquire);

        if (bottom - top > IntCapacity - 1)
        {
            return false;
        }

        m_queue[static_cast<size_t>(bottom % IntCapacity)] = std::move(value);

        std::atomic_thread_fence(std::memory_order_release);
        m_bottom.store(bottom + 1, std::memory_order_relaxed);

        return true;
    }

    bool pop(T& out)
    {
        const int64_t bottom = m_bottom.load(std::memory_order_relaxed) - 1;
        m_bottom.store(bottom, std::memory_order_relaxed);

        std::atomic_thread_fence(std::memory_order_seq_cst);

        int64_t top = m_top.load(std::memory_order_relaxed);

        bool valid = true;
        if (top <= bottom)
        {
            // The queue is not empty

            out = m_queue[static_cast<size_t>(bottom % IntCapacity)];
            if (top == bottom)
            {
                // if top == bottom, we're accessing the last element in the queue. We have to make sure the
                // syncronization between the stealer thread is mantained
                if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
                {
                    // The stealer thread got it first :(
                    valid = false;
                }

                m_bottom.store(bottom + 1, std::memory_order_relaxed);
            }
        }
        else
        {
            // Empty queue

            m_bottom.store(bottom + 1, std::memory_order_relaxed);
            valid = false;
        }

        return valid;
    }

    bool steal(T& out)
    {
        int64_t top = m_top.load(std::memory_order_acquire);
        std::atomic_thread_fence(std::memory_order_seq_cst);

        const int64_t bottom = m_bottom.load(std::memory_order_acquire);

        bool valid = false;
        if (top < bottom)
        {
            // The queue is not empty

            valid = true;
            out = m_queue[static_cast<size_t>(top % IntCapacity)];

            if (!m_top.compare_exchange_strong(top, top + 1, std::memory_order_seq_cst, std::memory_order_relaxed))
            {
                // Someone modified top before us
                valid = false;
            }
        }

        return valid;
    }

  private:
    std::vector<T> m_queue;
    std::atomic<int64_t> m_top, m_bottom;
};

} // namespace Mizu
