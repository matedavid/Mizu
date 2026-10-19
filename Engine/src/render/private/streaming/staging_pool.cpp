#include "streaming/staging_pool.h"

#include <algorithm>

#include "base/debug/assert.h"
#include "base/math/math.h"

namespace Mizu
{

StagingPool::StagingPool(uint64_t capacity) : m_buffer(capacity, 0)
{
    MIZU_ASSERT(capacity > 0, "StagingPool capacity must be greater than 0");

    m_free_blocks.push_back({0, capacity});
}

std::optional<StagingBlock> StagingPool::allocate(uint64_t size, uint64_t alignment)
{
    MIZU_ASSERT(size > 0, "Can't allocate block of size 0");
    MIZU_ASSERT(alignment == 0 || alignment % 2 == 0, "Alignment must be a power of two, got {}", alignment);

    std::lock_guard lock(m_mutex);

    const uint64_t align = std::max(alignment, ALIGNMENT);
    const uint64_t reserved = math::align_up(size, ALIGNMENT);

    constexpr size_t npos = std::numeric_limits<size_t>::max();

    size_t best = npos;
    uint64_t best_start = 0;
    uint64_t best_padding = 0;

    for (size_t i = 0; i < m_free_blocks.size(); ++i)
    {
        const FreeBlock& free_block = m_free_blocks[i];

        const uint64_t start = math::align_up(free_block.offset, align);
        const uint64_t padding = start - free_block.offset;

        if (free_block.size < padding + reserved)
            continue;

        const bool better = (best == npos) || (free_block.size < m_free_blocks[best].size)
                            || (free_block.size == m_free_blocks[best].size && padding < best_padding);
        if (better)
        {
            best = i;
            best_start = start;
            best_padding = padding;
        }
    }

    if (best == npos)
    {
        return std::nullopt;
    }

    const FreeBlock& free_block = m_free_blocks[best];

    const FreeBlock lead{free_block.offset, best_start - free_block.offset};
    const FreeBlock trail{best_start + reserved, (free_block.offset + free_block.size) - (best_start + reserved)};

    m_free_blocks.erase(m_free_blocks.begin() + static_cast<std::ptrdiff_t>(best));
    size_t at = best;
    if (lead.size > 0)
        m_free_blocks.insert(m_free_blocks.begin() + static_cast<std::ptrdiff_t>(at++), lead);
    if (trail.size > 0)
        m_free_blocks.insert(m_free_blocks.begin() + static_cast<std::ptrdiff_t>(at), trail);

    uint8_t* ptr = m_buffer.data() + best_start;
    MIZU_ASSERT(
        reinterpret_cast<uintptr_t>(ptr) % align == 0,
        "Pool base is not aligned enough for a {}-byte aligned allocation",
        align);

    return StagingBlock{ptr, best_start, size, reserved};
}

void StagingPool::free(const StagingBlock& block)
{
    MIZU_ASSERT(block.is_valid() && block.reserved > 0, "Freeing an invalid StagingBlock");

    std::lock_guard lock(m_mutex);

    auto it = std::lower_bound(
        m_free_blocks.begin(), m_free_blocks.end(), block.offset, [](const FreeBlock& f, uint64_t offset) {
            return f.offset < offset;
        });

    MIZU_ASSERT(
        it == m_free_blocks.end() || block.offset + block.reserved <= it->offset,
        "StagingBlock overlaps a free block (double free?)");
    MIZU_ASSERT(
        it == m_free_blocks.begin() || (it - 1)->offset + (it - 1)->size <= block.offset,
        "StagingBlock overlaps a free block (double free?)");

    it = m_free_blocks.insert(it, FreeBlock{block.offset, block.reserved});

    // Merge with the next hole
    if (auto next = it + 1; next != m_free_blocks.end() && it->offset + it->size == next->offset)
    {
        it->size += next->size;
        m_free_blocks.erase(next);
    }

    // Merge with the previous hole
    if (it != m_free_blocks.begin())
    {
        auto prev = it - 1;
        if (prev->offset + prev->size == it->offset)
        {
            prev->size += it->size;
            m_free_blocks.erase(it);
        }
    }
}

} // namespace Mizu