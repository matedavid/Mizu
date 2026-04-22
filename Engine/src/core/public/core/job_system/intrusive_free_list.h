#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <limits>
#include <vector>

namespace Mizu
{

using IntrusiveFreeListIndex = uint32_t;

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
    static_assert(Capacity < std::numeric_limits<IntrusiveFreeListIndex>::max(), "Capacity exceeds addressable range");
    static_assert(IsValidIntrusiveFreeListType<T>, "T must contain pool_index and next_free members");

    inline static T s_invalid_record{};

    struct alignas(8) TaggedIndex
    {
        uint32_t index;
        uint32_t tag;

        bool operator==(const TaggedIndex&) const = default;
    };

    static_assert(sizeof(TaggedIndex) == sizeof(uint64_t));

  public:
    IntrusiveFreeList() : m_list(Capacity)
    {
        for (IntrusiveFreeListIndex i = 0; i < Capacity; ++i)
        {
            T& record = m_list[i];
            record.pool_index = i;
            record.next_free = (i != Capacity - 1) ? i + 1 : IntrusiveFreeListInvalidIndex;
        }

        m_free_head.store({0, 0}, std::memory_order_relaxed);
    }

    IntrusiveFreeListIndex allocate()
    {
        while (true)
        {
            TaggedIndex head = m_free_head.load(std::memory_order_acquire);
            if (head.index == IntrusiveFreeListInvalidIndex)
                return IntrusiveFreeListInvalidIndex;

            T& record = get_record(head.index);
            const uint32_t next = static_cast<uint32_t>(record.next_free);
            const TaggedIndex next_head{next, head.tag + 1};

            if (m_free_head.compare_exchange_weak(
                    head, next_head, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                record.next_free = IntrusiveFreeListInvalidIndex;
                return head.index;
            }
        }
    }

    void free(IntrusiveFreeListIndex index)
    {
        T& record = get_record(index);

        while (true)
        {
            TaggedIndex head = m_free_head.load(std::memory_order_acquire);
            record.next_free = head.index;

            const TaggedIndex new_head{static_cast<uint32_t>(index), head.tag};

            if (m_free_head.compare_exchange_weak(head, new_head, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                return;
            }
        }
    }

    T& get(IntrusiveFreeListIndex index)
    {
        if (index == IntrusiveFreeListInvalidIndex)
            return s_invalid_record;

        return get_record(index);
    }

    const T& get(IntrusiveFreeListIndex index) const
    {
        if (index == IntrusiveFreeListInvalidIndex)
            return s_invalid_record;

        return get_record(index);
    }

  private:
    std::vector<T> m_list;
    std::atomic<TaggedIndex> m_free_head;

    T& get_record(IntrusiveFreeListIndex index) { return m_list[static_cast<size_t>(index)]; }
    const T& get_record(IntrusiveFreeListIndex index) const { return m_list[static_cast<size_t>(index)]; }
};

} // namespace Mizu