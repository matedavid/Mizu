#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

#include "asset/asset_handle.h"
#include "render_core/allocators/gpu_page_allocator.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/queue.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu
{

class CommandBuffer;
class FrameLinearAllocator;
class SparseBindScheduler;
struct TextureAssetMetadata;

enum class ResourceBacking
{
    Placed,
    Sparse,
};

struct TextureStreamingPoolDescription
{
    uint32_t release_delay_frames = 3;
};

struct TextureUploadData
{
    uint32_t old_first_mip = 0;
    uint32_t new_first_mip = 0;
    std::span<const uint8_t> data{};
};

class TextureStreamingPool
{
  public:
    TextureStreamingPool(
        TextureStreamingPoolDescription desc,
        GpuPageAllocator& page_allocator,
        SparseBindScheduler& sparse_bind_scheduler);
    ~TextureStreamingPool();

    void update();

    void register_texture(TextureAssetHandle handle, const TextureAssetMetadata& metadata);
    void unregister_texture(TextureAssetHandle handle);

    bool update_residency_state(TextureAssetHandle handle, const TextureAssetMetadata& metadata, uint32_t first_mip);

    void record_gpu_upload(
        TextureAssetHandle handle,
        const TextureAssetMetadata& metadata,
        const TextureUploadData& upload,
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator);

    uint32_t get_coarsest_first_mip(TextureAssetHandle handle) const;
    ImageResourceView get_resource_view(TextureAssetHandle handle) const;

  private:
    TextureStreamingPoolDescription m_description{};
    GpuPageAllocator& m_page_allocator;
    SparseBindScheduler& m_sparse_bind_scheduler;

    struct TextureEntry
    {
        std::shared_ptr<ImageResource> texture = nullptr;
        // Only used for placed resources
        std::shared_ptr<ImageResource> old_texture = nullptr;

        ResourceBacking backing = ResourceBacking::Placed;
        uint32_t resident_mips = 0;

        uint32_t mip_tail_first_mip = 0;
        uint32_t base_first_mip = 0;
        std::vector<std::vector<GpuPage>> mip_pages{};
        std::vector<GpuPage> mip_tail_pages{};
    };

    std::unordered_map<TextureAssetHandle, TextureEntry> m_texture_entry_map{};

    struct PendingRelease
    {
        std::shared_ptr<ImageResource> texture = nullptr;
        std::vector<GpuPage> pages{};
        uint32_t frames = 0;
    };

    std::vector<PendingRelease> m_pending_releases{};

    static constexpr uint32_t MIP_TAIL = std::numeric_limits<uint32_t>::max();

    struct PendingPageRelease
    {
        TextureAssetHandle handle{};
        uint32_t mip = 0;
        std::vector<GpuPage> pages{};
        uint32_t frames = 0;
    };

    std::vector<PendingPageRelease> m_pending_page_releases{};

    bool update_placed_residency_state(
        TextureAssetHandle handle,
        TextureEntry& entry,
        const TextureAssetMetadata& metadata,
        uint32_t first_mip);
    bool update_sparse_residency_state(
        TextureAssetHandle handle,
        TextureEntry& entry,
        const TextureAssetMetadata& metadata,
        uint32_t first_mip);

    void record_placed_gpu_upload(
        TextureEntry& entry,
        const TextureAssetMetadata& metadata,
        const TextureUploadData& upload,
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator);
    void record_sparse_gpu_upload(
        TextureAssetHandle handle,
        TextureEntry& entry,
        const TextureAssetMetadata& metadata,
        const TextureUploadData& upload,
        CommandBuffer& command,
        FrameLinearAllocator& frame_allocator);

    std::vector<GpuPage>& get_sparse_pages(TextureEntry& entry, uint32_t mip);
    uint32_t count_sparse_pages(const TextureEntry& entry, uint32_t mip) const;

    bool acquire_sparse_pages(TextureAssetHandle handle, TextureEntry& entry, std::span<const uint32_t> mips);
    void release_sparse_pages(TextureAssetHandle handle, TextureEntry& entry, uint32_t mip);

    void enqueue_sparse_binds(const TextureEntry& entry, uint32_t mip);
    void enqueue_sparse_unbinds(ImageResource& texture, uint32_t mip);

    void release_delayed(std::shared_ptr<ImageResource> texture);
    void process_pending_page_releases();

    ImageDescription create_image_description(
        TextureAssetHandle handle,
        const TextureAssetMetadata& metadata,
        uint32_t first_mip);

    ResourceBacking select_resource_backing(TextureAssetHandle handle, const TextureAssetMetadata& metadata);

    TextureEntry* get_entry(TextureAssetHandle handle);
    const TextureEntry* get_entry(TextureAssetHandle handle) const;
};

} // namespace Mizu
