#pragma once

#include <cstdint>
#include <mutex>
#include <vector>

namespace Mizu
{

template <typename T>
class ThreadSafeRingBuffer
{
  public:
    ThreadSafeRingBuffer(size_t capacity) : m_capacity(capacity), m_data(m_capacity) {}

    bool push(T item)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

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
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_head != m_tail)
        {
            item = m_data[m_head];
            m_head = (m_head + 1) % m_capacity;

            return true;
        }

        return false;
    }

  private:
    size_t m_capacity;
    std::vector<T> m_data;

    size_t m_head = 0;
    size_t m_tail = 0;

    std::mutex m_mutex;
};

} // namespace Mizu