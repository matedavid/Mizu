#include "resources/cpu_loading_pool.h"

#include <algorithm>

namespace Mizu
{

static uint64_t cpu_loading_pool_align_up(uint64_t value, uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

bool CpuLoadingPool::init(uint64_t mesh_budget, uint64_t texture_budget)
{
    arena_init(m_mesh_arena, AssetType::Mesh, mesh_budget);
    arena_init(m_texture_arena, AssetType::Texture, texture_budget);

    return true;
}

bool CpuLoadingPool::is_mesh_loaded(MeshAssetHandle handle)
{
    return arena_is_loaded(m_mesh_arena, handle.get_id());
}

bool CpuLoadingPool::is_texture_loaded(TextureAssetHandle handle)
{
    return arena_is_loaded(m_texture_arena, handle.get_id());
}

std::optional<CpuAllocationHandle> CpuLoadingPool::get_mesh(MeshAssetHandle handle)
{
    return arena_get(m_mesh_arena, handle.get_id());
}

std::optional<CpuAllocationHandle> CpuLoadingPool::get_texture(TextureAssetHandle handle)
{
    return arena_get(m_texture_arena, handle.get_id());
}

CpuLoadAcquireResult CpuLoadingPool::acquire_mesh(MeshAssetHandle handle, uint64_t size, uint64_t alignment)
{
    return arena_acquire(m_mesh_arena, handle.get_id(), size, alignment);
}

CpuLoadAcquireResult CpuLoadingPool::acquire_texture(TextureAssetHandle handle, uint64_t size, uint64_t alignment)
{
    return arena_acquire(m_texture_arena, handle.get_id(), size, alignment);
}

void CpuLoadingPool::commit_mesh(MeshAssetHandle handle)
{
    arena_commit(m_mesh_arena, handle.get_id());
}

void CpuLoadingPool::commit_texture(TextureAssetHandle handle)
{
    arena_commit(m_texture_arena, handle.get_id());
}

void CpuLoadingPool::abort_mesh(MeshAssetHandle handle)
{
    arena_abort(m_mesh_arena, handle.get_id());
}

void CpuLoadingPool::abort_texture(TextureAssetHandle handle)
{
    arena_abort(m_texture_arena, handle.get_id());
}

void CpuLoadingPool::arena_init(Arena& arena, AssetType asset_type, uint64_t size_bytes)
{
    arena.asset_type = asset_type;

    arena.access_tick = 0;
    arena.next_generation = 1;

    arena.buffer.clear();
    arena.free_blocks.clear();
    arena.entries.clear();

    arena.buffer.resize(size_bytes);
    arena.free_blocks.push_back(FreeBlock{0, size_bytes});
}

std::optional<CpuAllocationHandle> CpuLoadingPool::arena_get(Arena& arena, uint64_t asset_id)
{
    std::lock_guard lock{arena.mutex};

    const auto it = arena.entries.find(asset_id);
    if (it == arena.entries.end() || it->second.state != CpuCacheEntryState::Ready)
        return std::nullopt;

    arena_touch_entry(arena, it->second);
    return arena_make_handle(arena, it->second);
}

bool CpuLoadingPool::arena_is_loaded(Arena& arena, uint64_t asset_id)
{
    std::lock_guard lock{arena.mutex};

    const auto it = arena.entries.find(asset_id);
    if (it == arena.entries.end() || it->second.state != CpuCacheEntryState::Ready)
        return false;

    arena_touch_entry(arena, it->second);
    return true;
}

CpuLoadAcquireResult CpuLoadingPool::arena_acquire(Arena& arena, uint64_t asset_id, uint64_t size, uint64_t alignment)
{
    if (size == 0 || alignment == 0)
        return CpuLoadAcquireResult{};

    std::lock_guard lock{arena.mutex};

    const auto existing_it = arena.entries.find(asset_id);
    if (existing_it != arena.entries.end())
    {
        arena_touch_entry(arena, existing_it->second);

        if (existing_it->second.state == CpuCacheEntryState::Ready)
            return CpuLoadAcquireResult{CpuLoadAcquireStatus::CacheHit, arena_make_handle(arena, existing_it->second)};

        return CpuLoadAcquireResult{CpuLoadAcquireStatus::PendingLoad, {}};
    }

    std::optional<uint64_t> offset = arena_allocate_block(arena, size, alignment);
    while (!offset.has_value())
    {
        if (!arena_evict_one_entry(arena))
            return CpuLoadAcquireResult{};

        offset = arena_allocate_block(arena, size, alignment);
    }

    CacheEntry entry{};
    entry.asset_id = asset_id;
    entry.generation = arena.next_generation++;
    entry.state = CpuCacheEntryState::Loading;
    entry.offset = *offset;
    entry.size = size;
    entry.alignment = alignment;

    arena_touch_entry(arena, entry);

    const auto [it, inserted] = arena.entries.emplace(asset_id, entry);
    if (!inserted)
        return CpuLoadAcquireResult{};

    return CpuLoadAcquireResult{CpuLoadAcquireStatus::LoadRequired, arena_make_handle(arena, it->second)};
}

void CpuLoadingPool::arena_commit(Arena& arena, uint64_t asset_id)
{
    std::lock_guard lock{arena.mutex};

    const auto it = arena.entries.find(asset_id);
    if (it == arena.entries.end())
        return;

    it->second.state = CpuCacheEntryState::Ready;
    arena_touch_entry(arena, it->second);
}

void CpuLoadingPool::arena_abort(Arena& arena, uint64_t asset_id)
{
    std::lock_guard lock{arena.mutex};

    const auto it = arena.entries.find(asset_id);
    if (it == arena.entries.end())
        return;

    arena_free_block(arena, it->second.offset, it->second.size);
    arena.entries.erase(it);
}

std::optional<uint64_t> CpuLoadingPool::arena_allocate_block(Arena& arena, uint64_t size, uint64_t alignment)
{
    if (size == 0 || alignment == 0 || arena.free_blocks.empty())
        return std::nullopt;

    for (size_t index = 0; index < arena.free_blocks.size(); ++index)
    {
        FreeBlock& block = arena.free_blocks[index];
        const uint64_t aligned_offset = cpu_loading_pool_align_up(block.offset, alignment);
        const uint64_t block_end = block.offset + block.size;

        if (aligned_offset > block_end)
            continue;

        const uint64_t aligned_padding = aligned_offset - block.offset;
        if (aligned_padding > block.size)
            continue;

        const uint64_t available_size = block.size - aligned_padding;
        if (available_size < size)
            continue;

        const uint64_t allocation_end = aligned_offset + size;
        const uint64_t trailing_size = block_end - allocation_end;

        if (aligned_padding != 0 && trailing_size != 0)
        {
            block.size = aligned_padding;
            arena.free_blocks.insert(
                arena.free_blocks.begin() + static_cast<std::ptrdiff_t>(index + 1),
                FreeBlock{allocation_end, trailing_size});
        }
        else if (aligned_padding != 0)
        {
            block.size = aligned_padding;
        }
        else if (trailing_size != 0)
        {
            block.offset = allocation_end;
            block.size = trailing_size;
        }
        else
        {
            arena.free_blocks.erase(arena.free_blocks.begin() + static_cast<std::ptrdiff_t>(index));
        }

        return aligned_offset;
    }

    return std::nullopt;
}

void CpuLoadingPool::arena_free_block(Arena& arena, uint64_t offset, uint64_t size)
{
    if (size == 0)
        return;

    FreeBlock free_block{offset, size};
    auto free_blocks_it = std::lower_bound(
        arena.free_blocks.begin(), arena.free_blocks.end(), free_block, [](const FreeBlock& a, const FreeBlock& b) {
            return a.offset < b.offset;
        });

    if (free_blocks_it != arena.free_blocks.end() && free_block.offset + free_block.size == free_blocks_it->offset)
    {
        free_block.size += free_blocks_it->size;
        free_blocks_it = arena.free_blocks.erase(free_blocks_it);
    }

    if (free_blocks_it != arena.free_blocks.begin())
    {
        auto prev_it = std::prev(free_blocks_it);
        if (prev_it->offset + prev_it->size == free_block.offset)
        {
            prev_it->size += free_block.size;

            if (free_blocks_it != arena.free_blocks.end() && prev_it->offset + prev_it->size == free_blocks_it->offset)
            {
                prev_it->size += free_blocks_it->size;
                arena.free_blocks.erase(free_blocks_it);
            }

            return;
        }
    }

    arena.free_blocks.insert(free_blocks_it, free_block);
}

CpuAllocationHandle CpuLoadingPool::arena_make_handle(Arena& arena, const CacheEntry& entry)
{
    return CpuAllocationHandle{
        .id = entry.asset_id,
        .generation = entry.generation,
        .asset_type = arena.asset_type,
        .data = {arena.buffer.data() + entry.offset, static_cast<size_t>(entry.size)},
    };
}

void CpuLoadingPool::arena_touch_entry(Arena& arena, CacheEntry& entry)
{
    entry.last_access_tick = ++arena.access_tick;
}

bool CpuLoadingPool::arena_evict_one_entry(Arena& arena)
{
    auto victim_it = arena.entries.end();

    for (auto it = arena.entries.begin(); it != arena.entries.end(); ++it)
    {
        if (it->second.state != CpuCacheEntryState::Ready)
            continue;

        if (victim_it == arena.entries.end() || it->second.last_access_tick < victim_it->second.last_access_tick)
            victim_it = it;
    }

    if (victim_it == arena.entries.end())
        return false;

    arena_free_block(arena, victim_it->second.offset, victim_it->second.size);
    arena.entries.erase(victim_it);
    return true;
}

} // namespace Mizu
