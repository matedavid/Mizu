#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"

namespace Mizu
{

struct CpuAllocationHandle
{
    uint64_t id = std::numeric_limits<uint64_t>::max();
    uint64_t generation = 0;

    AssetType asset_type = AssetType::Mesh;
    std::span<uint8_t> data{};

    bool is_valid() const { return id != std::numeric_limits<uint64_t>::max(); }
};

enum class CpuLoadAcquireStatus
{
    CacheHit,
    LoadRequired,
    PendingLoad,
    Failed,
};

struct CpuLoadAcquireResult
{
    CpuLoadAcquireStatus status = CpuLoadAcquireStatus::Failed;
    CpuAllocationHandle allocation{};
};

enum class CpuCacheEntryState
{
    Loading,
    Ready,
};

class CpuLoadingPool
{
  public:
    bool init(uint64_t mesh_budget, uint64_t texture_budget);

    bool is_mesh_loaded(MeshAssetHandle handle);
    bool is_texture_loaded(TextureAssetHandle handle);

    std::optional<CpuAllocationHandle> get_mesh(MeshAssetHandle handle);
    std::optional<CpuAllocationHandle> get_texture(TextureAssetHandle handle);

    CpuLoadAcquireResult acquire_mesh(
        MeshAssetHandle handle,
        uint64_t size,
        uint64_t alignment = alignof(std::max_align_t));
    CpuLoadAcquireResult acquire_texture(
        TextureAssetHandle handle,
        uint64_t size,
        uint64_t alignment = alignof(std::max_align_t));

    void commit_mesh(MeshAssetHandle handle);
    void commit_texture(TextureAssetHandle handle);

    void abort_mesh(MeshAssetHandle handle);
    void abort_texture(TextureAssetHandle handle);

  private:
    struct FreeBlock
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    struct CacheEntry
    {
        uint64_t asset_id = 0;
        uint64_t generation = 0;
        uint64_t last_access_tick = 0;
        CpuCacheEntryState state = CpuCacheEntryState::Loading;

        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t alignment = 0;
    };

    struct Arena
    {
        AssetType asset_type = AssetType::Mesh;
        mutable std::mutex mutex;

        uint64_t access_tick = 0;
        uint64_t next_generation = 1;

        std::vector<uint8_t> buffer;
        std::vector<FreeBlock> free_blocks;
        std::unordered_map<uint64_t, CacheEntry> entries;
    };

    Arena m_mesh_arena{};
    Arena m_texture_arena{};

    void arena_init(Arena& arena, AssetType asset_type, uint64_t size_bytes);
    std::optional<CpuAllocationHandle> arena_get(Arena& arena, uint64_t asset_id);
    bool arena_is_loaded(Arena& arena, uint64_t asset_id);
    CpuLoadAcquireResult arena_acquire(Arena& arena, uint64_t asset_id, uint64_t size, uint64_t alignment);
    void arena_commit(Arena& arena, uint64_t asset_id);
    void arena_abort(Arena& arena, uint64_t asset_id);

    static std::optional<uint64_t> arena_allocate_block(Arena& arena, uint64_t size, uint64_t alignment);
    static void arena_free_block(Arena& arena, uint64_t offset, uint64_t size);
    static CpuAllocationHandle arena_make_handle(Arena& arena, const CacheEntry& entry);

    void arena_touch_entry(Arena& arena, CacheEntry& entry);
    bool arena_evict_one_entry(Arena& arena);
};

} // namespace Mizu