#pragma once

#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

// TODO: I should really start implement these containers/algorithms on a shared place instead of implementing them 10
// times.

class FreeRangeAllocator
{
  public:
    FreeRangeAllocator() = default;

    bool init(size_t size_bytes)
    {
        if (size_bytes == 0)
            return false;

        m_data.clear();
        m_free_ranges.clear();

        m_size_bytes = size_bytes;

        m_data.resize(m_size_bytes);

        m_free_ranges.push_back({.offset = 0, .size_bytes = m_size_bytes});

        return true;
    }

    std::span<uint8_t> allocate(size_t size)
    {
        if (size == 0)
            return {};

        std::lock_guard lock(m_mutex);

        for (size_t i = 0; i < m_free_ranges.size(); ++i)
        {
            FreeRange& range = m_free_ranges[i];

            if (range.size_bytes >= size)
            {
                const size_t allocate_offset = range.offset;

                range.offset += size;
                range.size_bytes -= size;

                if (range.size_bytes == 0)
                {
                    const ptrdiff_t idiff = static_cast<ptrdiff_t>(i);
                    m_free_ranges.erase(m_free_ranges.begin() + idiff);
                }

                return std::span<uint8_t>(m_data.data() + allocate_offset, size);
            }
        }

        return {};
    }

    void free(std::span<uint8_t> block)
    {
        if (block.empty())
            return;

        std::lock_guard lock(m_mutex);

        const uint8_t* base_ptr = m_data.data();
        const uint8_t* block_ptr = block.data();

        MIZU_ASSERT(
            block_ptr >= base_ptr && (block_ptr + block.size()) <= (base_ptr + m_data.size()),
            "Attempted to free a span outside allocator bounds!");

        const size_t offset = static_cast<size_t>(block_ptr - base_ptr);

        insert_and_merge(FreeRange{.offset = offset, .size_bytes = block.size()});
    }

  private:
    size_t m_size_bytes{};

    struct FreeRange
    {
        size_t offset;
        size_t size_bytes;
    };

    std::vector<FreeRange> m_free_ranges{};
    std::mutex m_mutex{};

    std::vector<uint8_t> m_data{};

    void insert_and_merge(FreeRange range)
    {
        MIZU_ASSERT(range.size_bytes > 0, "Can't insert and merge empty range");

        if (m_free_ranges.empty())
        {
            m_free_ranges.push_back(range);
            return;
        }

        const auto lower_it = std::lower_bound(
            m_free_ranges.begin(), m_free_ranges.end(), range.offset, [](const FreeRange& a, size_t offset) {
                return a.offset < offset;
            });

        const size_t index = static_cast<size_t>(std::distance(m_free_ranges.begin(), lower_it));
        MIZU_ASSERT(index <= m_free_ranges.size(), "Invalid index for free ranges");

        // Try merge with prev
        if (index > 0)
        {
            FreeRange& prev = m_free_ranges[index - 1];

            if (prev.offset + prev.size_bytes == range.offset)
            {
                prev.size_bytes += range.size_bytes;

                if (index < m_free_ranges.size())
                {
                    FreeRange& next = m_free_ranges[index];
                    if (prev.offset + prev.size_bytes == next.offset)
                    {
                        prev.size_bytes += next.size_bytes;

                        const ptrdiff_t indexdiff = static_cast<ptrdiff_t>(index);
                        m_free_ranges.erase(m_free_ranges.begin() + indexdiff);
                    }
                }

                return;
            }
        }

        // Try merge with next
        if (index < m_free_ranges.size())
        {
            FreeRange& next = m_free_ranges[index];

            if (range.offset + range.size_bytes == next.offset)
            {
                next.offset = range.offset;
                next.size_bytes += range.size_bytes;

                return;
            }
        }

        // Insert at index
        const ptrdiff_t indexdiff = static_cast<ptrdiff_t>(index);
        m_free_ranges.insert(m_free_ranges.begin() + indexdiff, range);
    }
};

} // namespace Mizu