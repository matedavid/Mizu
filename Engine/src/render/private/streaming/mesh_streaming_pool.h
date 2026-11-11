#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

#include "asset/asset_handle.h"
#include "render_core/allocators/gpu_page_allocator.h"
#include "render_core/rhi/buffer_resource.h"

#include "render/resources/gpu_resource_types.h"
#include "streaming/streaming_pool.h"

namespace Mizu
{

class CommandBuffer;
class FrameLinearAllocator;
class SparseBindScheduler;
struct MeshAssetMetadata;

class BufferRangeAllocator
{
  public:
    bool init(uint64_t size);

    std::optional<uint64_t> allocate(uint64_t size, uint64_t alignment = alignof(std::max_align_t));
    bool try_grow(uint64_t offset, uint64_t size, uint64_t new_size);
    void free(uint64_t offset, uint64_t size);

  private:
    struct FreeRange
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    uint64_t m_capacity = 0;
    std::mutex m_mutex;
    std::vector<FreeRange> m_free_ranges;
};

struct MeshStreamingPoolDescription
{
    uint64_t vertex_buffer_size = 512ull * 1024 * 1024;
    uint64_t index_buffer_size = 256ull * 1024 * 1024;
    uint32_t release_delay_frames = 3;
};

struct MeshUploadData
{
    uint8_t old_first_lod = 0;
    uint8_t new_first_lod = 0;
    std::span<const uint8_t> data{};
};

class MeshStreamingPool
{
  public:
    MeshStreamingPool(
        MeshStreamingPoolDescription desc,
        GpuPageAllocator& page_allocator,
        SparseBindScheduler& sparse_bind_scheduler);
    ~MeshStreamingPool();

    void update();

    bool register_mesh(MeshAssetHandle handle, const MeshAssetMetadata& metadata);
    void unregister_mesh(MeshAssetHandle handle);

    bool update_residency_state(MeshAssetHandle handle, const MeshAssetMetadata& metadata, uint8_t first_lod);

    void record_gpu_upload(
        MeshAssetHandle handle,
        const MeshAssetMetadata& metadata,
        const MeshUploadData& upload,
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator);

    std::optional<GpuMeshAllocationHandle> get_allocation(MeshAssetHandle handle) const;

    ResourceBacking get_backing() const { return m_backing; }

    const std::shared_ptr<BufferResource>& get_vertex_buffer() const { return m_vertex_buffer; }
    const std::shared_ptr<BufferResource>& get_index_buffer() const { return m_index_buffer; }

  private:
    MeshStreamingPoolDescription m_description{};
    GpuPageAllocator& m_page_allocator;
    SparseBindScheduler& m_sparse_bind_scheduler;

    ResourceBacking m_backing = ResourceBacking::Placed;

    std::shared_ptr<BufferResource> m_vertex_buffer = nullptr;
    std::shared_ptr<BufferResource> m_index_buffer = nullptr;

    BufferRangeAllocator m_vertex_allocator;
    BufferRangeAllocator m_index_allocator;

    struct BufferRange
    {
        uint64_t offset = 0;
        uint64_t size = 0;
    };

    struct MeshEntry
    {
        BufferRange vertex_range{};
        BufferRange index_range{};

        uint64_t resident_vertex_size = 0;
        uint64_t resident_index_size = 0;
        uint8_t resident_lods = 0;

        std::optional<BufferRange> pending_copy_vertex_range{};
        std::optional<BufferRange> pending_copy_index_range{};
    };

    std::unordered_map<MeshAssetHandle, MeshEntry> m_mesh_entry_map{};

    struct PendingRangeRelease
    {
        BufferRangeAllocator* allocator = nullptr;
        BufferRange range{};
        uint32_t frames = 0;
    };

    std::vector<PendingRangeRelease> m_pending_range_releases{};

    struct PageSlot
    {
        GpuPage page{};
        uint32_t references = 0;
        uint32_t release_serial = 0;
        bool bound = false;
    };

    struct SparsePageTable
    {
        BufferResource* buffer = nullptr;
        std::vector<PageSlot> slots{};
    };

    SparsePageTable m_vertex_page_table{};
    SparsePageTable m_index_page_table{};

    struct PendingPageRelease
    {
        SparsePageTable* table = nullptr;
        uint64_t slot = 0;
        uint32_t serial = 0;
        uint32_t frames = 0;
    };

    std::vector<PendingPageRelease> m_pending_page_releases{};

    bool update_placed_residency_state(MeshEntry& entry, const MeshAssetMetadata& metadata, uint8_t first_lod);
    bool update_sparse_residency_state(MeshEntry& entry, const MeshAssetMetadata& metadata, uint8_t first_lod);

    bool grow_placed_range(
        BufferRangeAllocator& allocator,
        BufferRange& range,
        std::optional<BufferRange>& pending_copy_range,
        uint64_t new_size,
        uint64_t alignment);
    void rollback_placed_grow(
        BufferRangeAllocator& allocator,
        BufferRange& range,
        std::optional<BufferRange>& pending_copy_range,
        uint64_t old_size);

    void shrink_resident_ranges(MeshEntry& entry, uint64_t new_vertex_size, uint64_t new_index_size);

    uint64_t get_page_size() const { return m_page_allocator.get_page_size(); }
    std::pair<uint64_t, uint64_t> get_page_slot_span(const BufferRange& range, uint64_t size) const;

    bool acquire_pages(SparsePageTable& table, uint64_t first_slot, uint64_t last_slot);
    void release_pages(SparsePageTable& table, uint64_t first_slot, uint64_t last_slot);
    bool resize_resident_pages(SparsePageTable& table, const BufferRange& range, uint64_t old_size, uint64_t new_size);

    void release_range_delayed(BufferRangeAllocator& allocator, const BufferRange& range);
    void process_pending_range_releases();
    void process_pending_page_releases();

    MeshEntry* get_entry(MeshAssetHandle handle);
    const MeshEntry* get_entry(MeshAssetHandle handle) const;
};

} // namespace Mizu
