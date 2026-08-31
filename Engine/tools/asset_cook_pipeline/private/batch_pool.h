#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <thread>
#include <type_traits>
#include <vector>

#include "base/containers/inplace_vector.h"

#include "core/job_system/job_system.h"

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
    bool init(uint32_t num_batches, JobSystem* job_system = nullptr)
    {
        m_job_system = job_system;

        m_storage.resize(num_batches);
        m_free_list.reserve(num_batches);

        for (size_t i = 0; i < num_batches; ++i)
        {
            m_free_list.push_back(&m_storage[i]);
        }

        return true;
    }

    void register_pending_release(JobHandle handle)
    {
        std::lock_guard lock(m_mutex);
        m_last_dispatched_handle = handle;
    }

    BatchT* acquire()
    {
        for (;;)
        {
            JobHandle handle_to_wait{};

            {
                std::lock_guard lock(m_mutex);

                if (!m_free_list.empty())
                {
                    BatchT* batch = m_free_list.back();
                    m_free_list.pop_back();

                    return batch;
                }

                handle_to_wait = m_last_dispatched_handle;
            }

            if (m_job_system != nullptr && handle_to_wait.is_valid())
            {
                m_job_system->wait_for(handle_to_wait);
            }
            else
            {
                std::this_thread::yield();
            }
        }
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

    JobHandle m_last_dispatched_handle{};
    JobSystem* m_job_system = nullptr;

    std::mutex m_mutex;
};

} // namespace Mizu