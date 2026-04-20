#pragma once

#include <atomic>
#include <concepts>
#include <limits>
#include <vector>

namespace Mizu
{

using IntrusiveFreeListIndex = size_t;

static constexpr IntrusiveFreeListIndex IntrusiveFreeListInvalidIndex =
    std::numeric_limits<IntrusiveFreeListIndex>::max();

template <typename T>
concept IsValidIntrusiveFreeListType = requires(T t) {
    { t.pool_index } -> std::convertible_to<IntrusiveFreeListIndex>;
    { t.next_free } -> std::convertible_to<IntrusiveFreeListIndex>;
};

template <typename T, size_t Capacity>
class IntrusiveFreeList
{
    static_assert(Capacity != 0, "Invalid Capacity for IntrusiveFreeList");
    static_assert(IsValidIntrusiveFreeListType<T>, "T must contain pool_index and next_free members");

    inline static T s_invalid_record{};

  public:
    IntrusiveFreeList() : m_list(Capacity)
    {
        for (size_t i = 0; i < Capacity; ++i)
        {
            T& record = m_list[i];
            record.pool_index = i;
            record.next_free = (i != Capacity - 1) ? i + 1 : IntrusiveFreeListInvalidIndex;
        }

        m_free_head.store(0);
    }

    IntrusiveFreeListIndex allocate()
    {
        while (true)
        {
            IntrusiveFreeListIndex head = m_free_head.load(std::memory_order_acquire);
            if (head == IntrusiveFreeListInvalidIndex)
                return IntrusiveFreeListInvalidIndex; // Pool is full

            T& record = get_record(head);
            const IntrusiveFreeListIndex next_free = record.next_free;

            if (m_free_head.compare_exchange_weak(
                    head, next_free, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                record.next_free = IntrusiveFreeListInvalidIndex;
                return head;
            }
        }

        return IntrusiveFreeListInvalidIndex;
    }

    void free(IntrusiveFreeListIndex index)
    {
        T& record = get_record(index);

        while (true)
        {
            IntrusiveFreeListIndex head = m_free_head.load(std::memory_order_acquire);
            record.next_free = head;

            if (m_free_head.compare_exchange_weak(head, index, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                return;
            }
        }
    }

    T& get(IntrusiveFreeListIndex index)
    {
        if (index == IntrusiveFreeListInvalidIndex)
        {
            return s_invalid_record;
        }

        return get_record(index);
    }

    const T& get(IntrusiveFreeListIndex index) const
    {
        if (index == IntrusiveFreeListInvalidIndex)
        {
            return s_invalid_record;
        }

        return get_record(index);
    }

  private:
    std::vector<T> m_list;
    std::atomic<IntrusiveFreeListIndex> m_free_head;

    T& get_record(IntrusiveFreeListIndex index) { return m_list[static_cast<size_t>(index)]; }
    const T& get_record(IntrusiveFreeListIndex index) const { return m_list[static_cast<size_t>(index)]; }
};

} // namespace Mizu
