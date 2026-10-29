#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <span>
#include <vector>

#include "base/containers/inplace_vector.h"

namespace Mizu
{

template <typename T, size_t Size = 32>
class Batch;

template <typename T, size_t Size>
class Batch
{
  public:
    void add(T&& item) { m_data.push_back(std::move(item)); }
    void add(const T& item) { m_data.push_back(item); }

    void clear() { m_data.clear(); }

    bool is_full() const { return m_data.size() == Size; }
    bool is_empty() const { return m_data.empty(); }

    std::span<T> get_span() { return m_data; }

  private:
    inplace_vector<T, Size> m_data;
};

template <typename T, size_t Size = 32>
class PipelineStage
{
  public:
    using RequestType = T;
    using BatchType = Batch<T, Size>;

    bool init(uint32_t max_in_flight)
    {
        if (max_in_flight == 0)
            return false;

        std::lock_guard lock(m_mutex);

        m_queue.clear();
        m_storage = std::vector<BatchType>(max_in_flight);

        m_free_slots.clear();
        m_free_slots.reserve(max_in_flight);
        for (BatchType& batch : m_storage)
        {
            m_free_slots.push_back(&batch);
        }

        return true;
    }

    void push(T&& request)
    {
        std::lock_guard lock(m_mutex);
        m_queue.push_back(std::move(request));
    }

    BatchType* try_take_batch()
    {
        std::lock_guard lock(m_mutex);

        if (m_queue.empty() || m_free_slots.empty())
            return nullptr;

        BatchType* batch = m_free_slots.back();
        m_free_slots.pop_back();

        while (!batch->is_full() && !m_queue.empty())
        {
            batch->add(std::move(m_queue.front()));
            m_queue.pop_front();
        }

        return batch;
    }

    void release(BatchType* batch)
    {
        batch->clear();

        std::lock_guard lock(m_mutex);
        m_free_slots.push_back(batch);
    }

    size_t backlog() const
    {
        std::lock_guard lock(m_mutex);
        return m_queue.size();
    }

  private:
    std::vector<BatchType> m_storage{};
    std::vector<BatchType*> m_free_slots{};
    std::deque<T> m_queue{};

    mutable std::mutex m_mutex;
};

} // namespace Mizu