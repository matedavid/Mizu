#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <type_traits>
#include <vector>

#include "base/containers/inplace_vector.h"

namespace Mizu
{

template <typename T, size_t Size = 32>
class Batch;

template <typename T>
struct is_batch : std::false_type
{
};

template <typename ItemType, size_t Size>
struct is_batch<Batch<ItemType, Size>> : std::true_type
{
};

template <typename T>
concept BatchPoolBatch = is_batch<T>::value;

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

template <BatchPoolBatch BatchT>
class BoundedBatchPool
{
  public:
    bool init(uint32_t num_batches)
    {
        m_storage.resize(num_batches);
        m_free_list.reserve(num_batches);

        for (size_t i = 0; i < num_batches; ++i)
        {
            m_free_list.push_back(&m_storage[i]);
        }

        return true;
    }

    BatchT* acquire()
    {
        std::lock_guard lock(m_mutex);

        BatchT* batch = m_free_list.back();
        m_free_list.pop_back();

        return batch;
    }

    void release(BatchT* batch)
    {
        batch->clear();

        std::lock_guard lock(m_mutex);
        m_free_list.push_back(batch);
    }

  private:
    std::vector<BatchT> m_storage{};
    std::vector<BatchT*> m_free_list{};

    std::mutex m_mutex;
};

} // namespace Mizu