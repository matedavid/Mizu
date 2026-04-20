#pragma once

#include <atomic>
#include <vector>

namespace Mizu
{

template <typename T, size_t Capacity>
class MpscQueue
{
    static_assert(Capacity >= 2, "Capacity too small");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of 2");

  public:
    MpscQueue() : m_queue(Capacity), m_head(0), m_tail(0)
    {
        for (size_t i = 0; i < Capacity; ++i)
        {
            Slot& slot = m_queue[i];
            slot.seq.store(i);
        }
    }

    bool push(T value)
    {
        size_t pos = m_tail.load(std::memory_order_relaxed);

        while (true)
        {
            Slot& slot = m_queue[pos & ModuloMask];
            const size_t seq = slot.seq.load(std::memory_order_acquire);

            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
            if (diff == 0
                && m_tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed, std::memory_order_relaxed))
            {
                slot.value = value;
                slot.seq.store(pos + 1, std::memory_order_release);

                return true;
            }
            else if (diff < 0)
            {
                // Slot is behind consumer, queue is full
                return false;
            }
            else
            {
                // Another producer advanced the tail, retry
                pos = m_tail.load(std::memory_order_relaxed);
            }
        }

        return false;
    }

    bool pop(T& out)
    {
        const size_t pos = m_head.load(std::memory_order_relaxed);
        Slot& slot = m_queue[pos & ModuloMask];

        // seq == pos+1 means published and ready
        const size_t seq = slot.seq.load(std::memory_order_acquire);
        if (seq != pos + 1)
        {
            // Queue is empty
            return false;
        }

        out = std::move(slot.value);

        slot.seq.store(pos + Capacity, std::memory_order_release);
        m_head.store(pos + 1, std::memory_order_relaxed);

        return true;
    }

  private:
    struct Slot
    {
        T value;
        std::atomic<size_t> seq;
    };

    static constexpr size_t ModuloMask = Capacity - 1;

    std::vector<Slot> m_queue;
    std::atomic<size_t> m_head, m_tail;
};

} // namespace Mizu
