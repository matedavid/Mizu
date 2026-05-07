#pragma once

#include <atomic>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

namespace Mizu
{

template <typename T, typename IndexT>
concept IsValidIntrusiveFreeListType = requires(T t) {
    { t.pool_index } -> std::convertible_to<IndexT>;
    { t.next_free } -> std::convertible_to<IndexT>;
};

template <typename Tag>
struct IntrusiveFreeListIndex
{
    using ValueT = uint32_t;

    static constexpr ValueT InvalidValue = std::numeric_limits<ValueT>::max();
    ValueT value = InvalidValue;

    constexpr IntrusiveFreeListIndex() = default;
    constexpr explicit IntrusiveFreeListIndex(ValueT value_) : value(value_) {}

    constexpr bool operator==(const IntrusiveFreeListIndex<Tag>& other) const { return value == other.value; }
    constexpr bool is_valid() const { return value != InvalidValue; }
};

// clang-format off
template <typename Tag>
concept IsIntrusiveFreeListIndexValid =
    sizeof(IntrusiveFreeListIndex<Tag>) == sizeof(uint32_t) 
    && alignof(IntrusiveFreeListIndex<Tag>) == alignof(uint32_t)
    && std::is_trivially_copyable_v<IntrusiveFreeListIndex<Tag>>;
// clang-format on

template <typename T, size_t Capacity, typename IndexTag>
class IntrusiveFreeList
{
  private:
    using Index = IntrusiveFreeListIndex<IndexTag>;

    static_assert(Capacity != 0, "Invalid Capacity for IntrusiveFreeList");
    static_assert(Capacity < std::numeric_limits<typename Index::ValueT>::max(), "Capacity exceeds addressable range");
    static_assert(IsIntrusiveFreeListIndexValid<IndexTag>, "Index must remain a trivially copyable uint32_t wrapper");
    static_assert(IsValidIntrusiveFreeListType<T, Index>, "T must contain pool_index and next_free members");

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
        for (typename Index::ValueT i = 0; i < Capacity; ++i)
        {
            T& record = m_list[i];
            record.pool_index = Index{i};
            record.next_free = (i != Capacity - 1) ? Index{i + 1} : Index{};
        }

        m_free_head.store({0, 0}, std::memory_order_relaxed);
    }

    Index allocate()
    {
        while (true)
        {
            TaggedIndex head = m_free_head.load(std::memory_order_acquire);
            if (head.index == Index::InvalidValue)
                return Index{};

            T& record = get_record(Index{head.index});
            const uint32_t next = record.next_free.value;
            const TaggedIndex next_head{next, head.tag + 1};

            if (m_free_head.compare_exchange_weak(
                    head, next_head, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                record.next_free = Index{};
                return Index{head.index};
            }
        }
    }

    void free(Index index)
    {
        T& record = get_record(index);

        while (true)
        {
            TaggedIndex head = m_free_head.load(std::memory_order_acquire);
            record.next_free = Index{head.index};

            const TaggedIndex new_head{index.value, head.tag + 1};

            if (m_free_head.compare_exchange_weak(head, new_head, std::memory_order_acq_rel, std::memory_order_acquire))
            {
                return;
            }
        }
    }

    T& get(Index index)
    {
        if (!index.is_valid())
            return s_invalid_record;

        return get_record(index);
    }

    const T& get(Index index) const
    {
        if (!index.is_valid())
            return s_invalid_record;

        return get_record(index);
    }

  private:
    std::vector<T> m_list;
    std::atomic<TaggedIndex> m_free_head;

    T& get_record(Index index) { return m_list[static_cast<size_t>(index.value)]; }
    const T& get_record(Index index) const { return m_list[static_cast<size_t>(index.value)]; }
};

} // namespace Mizu