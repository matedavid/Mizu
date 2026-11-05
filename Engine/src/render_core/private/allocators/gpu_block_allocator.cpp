#include "render_core/allocators/gpu_block_allocator.h"

#include <algorithm>
#include <format>
#include <limits>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/math/math.h"

#include "render_core/rhi/device.h"

namespace Mizu
{

static uint64_t make_pool_key(HeapDomain domain, MemoryTypeId memory_type, GpuResourceKind kind)
{
    // clang-format off
    return (static_cast<uint64_t>(memory_type) << 16)
         | (static_cast<uint64_t>(domain) << 8)
         |  static_cast<uint64_t>(kind);
    // clang-format on
}

GpuBlockAllocator::GpuBlockAllocator(const Device& device, GpuBlockAllocatorDescription desc)
    : GpuAllocator(device)
    , m_description(std::move(desc))
{
}

GpuBlockAllocator::~GpuBlockAllocator()
{
    MIZU_ASSERT(
        m_num_allocations == 0,
        "GpuBlockAllocator '{}' destroyed with {} live allocations, resources created through it must be destroyed "
        "before the allocator",
        m_description.name,
        m_num_allocations);
}

std::optional<GpuAllocation> GpuBlockAllocator::allocate(
    const MemoryRequirements& reqs,
    HeapDomain domain,
    GpuResourceKind kind)
{
    MIZU_ASSERT(reqs.size > 0, "Trying to allocate 0 bytes");

    const std::optional<MemoryTypeId> memory_type = m_device.find_memory_type(reqs, domain);
    if (!memory_type.has_value())
        return std::nullopt;

    const uint64_t alignment = std::max<uint64_t>(reqs.alignment, 1);
    const uint64_t pool_key = make_pool_key(domain, *memory_type, kind);
    const uint64_t block_size = domain == HeapDomain::DeviceLocal ? m_description.device_local_block_size
                                                                  : m_description.host_visible_block_size;

    std::lock_guard lock(m_mutex);

    if (reqs.dedicated != DedicatedAllocation::None || reqs.size > block_size / 2)
    {
        const uint32_t block_id = create_block(domain, *memory_type, reqs.size, pool_key, true);
        Block& block = m_blocks.at(block_id);
        reserve_range(block, 0, reqs.size);

        m_num_allocations += 1;

        return GpuAllocation{
            .placement = MemoryPlacement{.heap = block.heap.get(), .offset = 0},
            .size = reqs.size,
            .handle = block_id,
        };
    }

    std::vector<uint32_t>& pool = m_pools[pool_key];

    uint32_t best_block_id = 0;
    uint64_t best_offset = 0;
    uint64_t best_waste = std::numeric_limits<uint64_t>::max();

    for (const uint32_t block_id : pool)
    {
        uint64_t waste = 0;
        const std::optional<uint64_t> offset = find_range(m_blocks.at(block_id), reqs.size, alignment, waste);
        if (offset.has_value() && waste < best_waste)
        {
            best_block_id = block_id;
            best_offset = *offset;
            best_waste = waste;
        }
    }

    if (best_block_id == 0)
    {
        best_block_id = create_block(domain, *memory_type, block_size, pool_key, false);
        pool.push_back(best_block_id);
        best_offset = 0;
    }

    Block& block = m_blocks.at(best_block_id);
    reserve_range(block, best_offset, reqs.size);

    m_num_allocations += 1;

    return GpuAllocation{
        .placement = MemoryPlacement{.heap = block.heap.get(), .offset = best_offset},
        .size = reqs.size,
        .handle = best_block_id,
    };
}

void GpuBlockAllocator::release(const GpuAllocation& allocation)
{
    std::lock_guard lock(m_mutex);

    const uint32_t block_id = static_cast<uint32_t>(allocation.handle);

    const auto it = m_blocks.find(block_id);
    MIZU_ASSERT(it != m_blocks.end(), "Releasing an allocation that does not belong to '{}'", m_description.name);

    Block& block = it->second;
    MIZU_ASSERT(block.heap.get() == allocation.placement.heap, "Allocation heap does not match its block");

    free_range(block, allocation.placement.offset, allocation.size);
    m_num_allocations -= 1;

    if (block.used != 0)
        return;

    if (block.dedicated)
    {
        destroy_block(block_id);
        return;
    }

    // Keep one empty block per pool around to avoid creating and destroying heaps continuously
    std::vector<uint32_t>& pool = m_pools.at(block.pool_key);
    const auto num_empty = std::ranges::count_if(pool, [&](uint32_t id) { return m_blocks.at(id).used == 0; });

    if (num_empty > 1)
    {
        std::erase(pool, block_id);
        destroy_block(block_id);
    }
}

uint64_t GpuBlockAllocator::get_num_allocations() const
{
    std::lock_guard lock(m_mutex);
    return m_num_allocations;
}

uint64_t GpuBlockAllocator::get_num_heaps() const
{
    std::lock_guard lock(m_mutex);
    return m_blocks.size();
}

uint64_t GpuBlockAllocator::get_allocated_heap_size() const
{
    std::lock_guard lock(m_mutex);
    return m_allocated_heap_size;
}

uint32_t GpuBlockAllocator::create_block(
    HeapDomain domain,
    MemoryTypeId memory_type,
    uint64_t size,
    uint64_t pool_key,
    bool dedicated)
{
    const uint32_t block_id = m_next_block_id++;

    GpuHeapDescription heap_desc{};
    heap_desc.size = size;
    heap_desc.domain = domain;
    heap_desc.memory_type = memory_type;
    heap_desc.name = dedicated ? std::format("{}_Dedicated_{}", m_description.name, block_id)
                               : std::format("{}_Block_{}", m_description.name, block_id);

    Block block{};
    block.heap = m_device.create_gpu_heap(heap_desc);
    block.free_ranges.emplace(0, size);
    block.pool_key = pool_key;
    block.dedicated = dedicated;

    m_allocated_heap_size += block.heap->get_size();
    m_blocks.emplace(block_id, std::move(block));

    return block_id;
}

void GpuBlockAllocator::destroy_block(uint32_t block_id)
{
    const auto it = m_blocks.find(block_id);
    MIZU_ASSERT(it != m_blocks.end(), "Invalid block id {}", block_id);

    m_allocated_heap_size -= it->second.heap->get_size();
    m_blocks.erase(it);
}

std::optional<uint64_t> GpuBlockAllocator::find_range(
    const Block& block,
    uint64_t size,
    uint64_t alignment,
    uint64_t& waste)
{
    std::optional<uint64_t> best_offset;
    uint64_t best_waste = std::numeric_limits<uint64_t>::max();

    for (const auto& [range_offset, range_size] : block.free_ranges)
    {
        const uint64_t offset = math::align_up(range_offset, alignment);
        const uint64_t range_end = range_offset + range_size;

        if (offset + size > range_end)
            continue;

        const uint64_t range_waste = range_size - size;
        if (range_waste < best_waste)
        {
            best_offset = offset;
            best_waste = range_waste;
        }
    }

    waste = best_waste;
    return best_offset;
}

void GpuBlockAllocator::reserve_range(Block& block, uint64_t offset, uint64_t size)
{
    auto it = block.free_ranges.upper_bound(offset);
    MIZU_ASSERT(it != block.free_ranges.begin(), "Reserving a range that is not free");
    --it;

    const uint64_t range_offset = it->first;
    const uint64_t range_end = it->first + it->second;
    MIZU_ASSERT(offset >= range_offset && offset + size <= range_end, "Reserving a range that is not free");

    block.free_ranges.erase(it);

    if (offset > range_offset)
        block.free_ranges.emplace(range_offset, offset - range_offset);
    if (offset + size < range_end)
        block.free_ranges.emplace(offset + size, range_end - (offset + size));

    block.used += size;
}

void GpuBlockAllocator::free_range(Block& block, uint64_t offset, uint64_t size)
{
    MIZU_ASSERT(block.used >= size, "Freeing more memory than used in the block");

    auto it = block.free_ranges.emplace(offset, size).first;

    if (const auto next = std::next(it); next != block.free_ranges.end())
    {
        MIZU_ASSERT(offset + size <= next->first, "Freed range overlaps a free range (double free?)");
        if (offset + size == next->first)
        {
            it->second += next->second;
            block.free_ranges.erase(next);
        }
    }

    if (it != block.free_ranges.begin())
    {
        const auto prev = std::prev(it);
        MIZU_ASSERT(prev->first + prev->second <= offset, "Freed range overlaps a free range (double free?)");
        if (prev->first + prev->second == offset)
        {
            prev->second += it->second;
            block.free_ranges.erase(it);
        }
    }

    block.used -= size;
}

} // namespace Mizu
