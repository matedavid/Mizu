#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

template <typename T>
class ThreadSafeRingBuffer
{
  public:
    ThreadSafeRingBuffer() = default;

    ThreadSafeRingBuffer(const ThreadSafeRingBuffer& other)
    {
        m_capacity = other.m_capacity;
        m_data = other.m_data;
        m_head = other.m_head;
        m_tail = other.m_tail;
    }

    ThreadSafeRingBuffer& operator=(const ThreadSafeRingBuffer& other)
    {
        if (this == &other)
            return *this;

        m_capacity = other.m_capacity;
        m_data = other.m_data;
        m_head = other.m_head;
        m_tail = other.m_tail;

        return *this;
    }

    void init(size_t capacity)
    {
        MIZU_ASSERT(capacity != 0, "Can't create RingBuffer with capacity = 0");
        MIZU_ASSERT(m_capacity == 0, "RingBuffer has already been initialized");

        m_head = 0;
        m_tail = 0;

        m_capacity = capacity;
        m_data.resize(m_capacity);
    }

    bool push(T item)
    {
        std::lock_guard lock(m_mutex);

        const size_t next = (m_tail + 1) % m_capacity;
        if (next != m_head)
        {
            m_data[m_tail] = item;
            m_tail = next;

            return true;
        }

        return false;
    }

    bool pop(T& item)
    {
        std::lock_guard lock(m_mutex);

        if (m_head != m_tail)
        {
            item = m_data[m_head];
            m_head = (m_head + 1) % m_capacity;

            return true;
        }

        return false;
    }

    bool empty() const
    {
        std::lock_guard lock(m_mutex);
        return m_head == m_tail;
    }

    void reset()
    {
        std::lock_guard lock(m_mutex);

        m_head = 0;
        m_tail = 0;
    }

  private:
    size_t m_capacity = 0;
    std::vector<T> m_data{};

    size_t m_head = 0;
    size_t m_tail = 0;

    mutable std::mutex m_mutex{};
};

} // namespace Mizu