#include "resources/gpu_pools.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <ranges>
#include <string>

#include "asset/asset.h"
#include "asset/asset_metadata.h"
#include "asset/asset_registry.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/math/math.h"
#include "base/reflection/enum_traits.h"
#include "core/game_context.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/device.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/memory.h"

#include "render/runtime/renderer.h"
#include "render/systems/frame_linear_allocator.h"
#include "streaming/sparse_bind_scheduler.h"

namespace Mizu
{

bool BufferRangeAllocator::init(uint64_t size)
{
    std::lock_guard lock{m_mutex};

    m_capacity = size;
    m_free_ranges.clear();

    if (size != 0)
        m_free_ranges.push_back(FreeRange{0, size});

    return size != 0;
}

std::optional<uint64_t> BufferRangeAllocator::allocate(uint64_t size, uint64_t alignment)
{
    if (size == 0 || alignment == 0)
        return std::nullopt;

    std::lock_guard lock{m_mutex};

    for (size_t index = 0; index < m_free_ranges.size(); ++index)
    {
        FreeRange& range = m_free_ranges[index];
        const uint64_t aligned_offset = math::align_up(range.offset, alignment);
        const uint64_t range_end = range.offset + range.size;

        if (aligned_offset > range_end)
            continue;

        const uint64_t aligned_padding = aligned_offset - range.offset;
        if (aligned_padding > range.size)
            continue;

        const uint64_t available_size = range.size - aligned_padding;
        if (available_size < size)
            continue;

        const uint64_t allocation_end = aligned_offset + size;
        const uint64_t trailing_size = range_end - allocation_end;

        if (aligned_padding != 0 && trailing_size != 0)
        {
            range.size = aligned_padding;
            m_free_ranges.insert(
                m_free_ranges.begin() + static_cast<std::ptrdiff_t>(index + 1),
                FreeRange{allocation_end, trailing_size});
        }
        else if (aligned_padding != 0)
        {
            range.size = aligned_padding;
        }
        else if (trailing_size != 0)
        {
            range.offset = allocation_end;
            range.size = trailing_size;
        }
        else
        {
            m_free_ranges.erase(m_free_ranges.begin() + static_cast<std::ptrdiff_t>(index));
        }

        return aligned_offset;
    }

    return std::nullopt;
}

void BufferRangeAllocator::free(uint64_t offset, uint64_t size)
{
    if (size == 0)
        return;

    std::lock_guard lock{m_mutex};

    MIZU_ASSERT(offset + size <= m_capacity, "Trying to free range out of bounds on BufferRangeAllocator");

    FreeRange free_range{offset, size};
    auto free_ranges_it = std::lower_bound(
        m_free_ranges.begin(), m_free_ranges.end(), free_range, [](const FreeRange& a, const FreeRange& b) {
            return a.offset < b.offset;
        });

    if (free_ranges_it != m_free_ranges.end() && free_range.offset + free_range.size == free_ranges_it->offset)
    {
        free_range.size += free_ranges_it->size;
        free_ranges_it = m_free_ranges.erase(free_ranges_it);
    }

    if (free_ranges_it != m_free_ranges.begin())
    {
        auto prev_it = std::prev(free_ranges_it);
        if (prev_it->offset + prev_it->size == free_range.offset)
        {
            prev_it->size += free_range.size;

            if (free_ranges_it != m_free_ranges.end() && prev_it->offset + prev_it->size == free_ranges_it->offset)
            {
                prev_it->size += free_ranges_it->size;
                m_free_ranges.erase(free_ranges_it);
            }

            return;
        }
    }

    m_free_ranges.insert(free_ranges_it, free_range);
}

//
// GpuMeshPool
//

bool GpuMeshPool::init(uint64_t size)
{
    BufferDescription vertex_buffer_desc =
        create_vertex_buffer_desc(size, sizeof(MeshAssetVertex), "GpuMeshPool_VertexBuffer");
    vertex_buffer_desc.usage |= BufferUsageBits::TransferDst;
    m_vertex_buffer = g_render_device->create_buffer(vertex_buffer_desc);

    BufferDescription index_buffer_desc = create_index_buffer_desc(size, "GpuMeshPool_IndexBuffer");
    index_buffer_desc.usage |= BufferUsageBits::TransferDst;
    m_index_buffer = g_render_device->create_buffer(index_buffer_desc);

    const bool buffers_initialized = m_vertex_buffer != nullptr && m_index_buffer != nullptr;
    if (!buffers_initialized)
        return false;

    return m_vertex_allocator.init(size) && m_index_allocator.init(size);
}

std::optional<GpuMeshAllocationHandle> GpuMeshPool::allocate(
    const MeshAssetHandle& handle,
    uint64_t vertex_size,
    uint64_t index_size,
    uint64_t vertex_alignment,
    uint64_t index_alignment)
{
    MIZU_ASSERT(handle.is_valid(), "Trying to allocate invalid MeshAssetHandle from GpuMeshPool");

    const std::optional<uint64_t> vertex_offset = m_vertex_allocator.allocate(vertex_size, vertex_alignment);
    if (!vertex_offset.has_value())
        return std::nullopt;

    const std::optional<uint64_t> index_offset = m_index_allocator.allocate(index_size, index_alignment);
    if (!index_offset.has_value())
    {
        m_vertex_allocator.free(*vertex_offset, vertex_size);
        return std::nullopt;
    }

    return GpuMeshAllocationHandle{
        .handle = handle,
        .vertex_offset = *vertex_offset,
        .vertex_size = vertex_size,
        .index_offset = *index_offset,
        .index_size = index_size,
    };
}

void GpuMeshPool::free(const GpuMeshAllocationHandle& allocation)
{
    if (allocation.vertex_size != 0)
        m_vertex_allocator.free(allocation.vertex_offset, allocation.vertex_size);

    if (allocation.index_size != 0)
        m_index_allocator.free(allocation.index_offset, allocation.index_size);
}

// TODO: Get this from the device, it's the global toggle if sparse resources are supported
static bool s_sparse_resources_supported = true;

GpuTexturePool::GpuTexturePool(
    GpuTexturePoolDescription desc,
    GpuPageAllocator& page_allocator,
    SparseBindScheduler& sparse_bind_scheduler)
    : m_description(std::move(desc))
    , m_page_allocator(page_allocator)
    , m_sparse_bind_scheduler(sparse_bind_scheduler)
{
}

GpuTexturePool::~GpuTexturePool()
{
    for (TextureEntry& entry : m_texture_entry_map | std::views::values)
    {
        for (const std::vector<GpuPage>& pages : entry.mip_pages)
        {
            m_page_allocator.free_pages(pages);
        }

        m_page_allocator.free_pages(entry.mip_tail_pages);
    }

    for (const PendingPageRelease& release : m_pending_page_releases)
    {
        m_page_allocator.free_pages(release.pages);
    }

    for (const PendingRelease& release : m_pending_releases)
    {
        m_page_allocator.free_pages(release.pages);
    }
}

void GpuTexturePool::update()
{
    auto it = m_pending_releases.begin();
    while (it != m_pending_releases.end())
    {
        it->frames -= 1;

        if (it->frames == 0)
        {
            m_page_allocator.free_pages(it->pages);
            it = m_pending_releases.erase(it);
        }
        else
        {
            it = std::next(it);
        }
    }

    process_pending_page_releases();
}

void GpuTexturePool::register_texture(TextureAssetHandle handle, const TextureAssetMetadata& metadata)
{
    const ResourceBacking backing = select_resource_backing(handle, metadata);

    TextureEntry entry{};
    entry.backing = backing;
    entry.resident_mips = 0;

    if (backing == ResourceBacking::Sparse)
    {
        const ImageDescription desc = create_image_description(handle, metadata, 0);
        entry.texture = g_render_device->create_sparse_image(desc);

        const SparseImageMemoryRequirements reqs = entry.texture->get_sparse_memory_requirements();
        entry.mip_tail_first_mip = std::min(reqs.mip_tail_first_mip, metadata.num_mips);
        entry.mip_pages.resize(entry.mip_tail_first_mip);

        const uint32_t requested_streamable_mips =
            std::min(static_cast<uint32_t>(metadata.streamable_mips), metadata.num_mips - 1);
        const uint32_t effective_streamable_mips = std::min(requested_streamable_mips, entry.mip_tail_first_mip);

        entry.base_first_mip = effective_streamable_mips;

        if (effective_streamable_mips < requested_streamable_mips)
        {
            MIZU_LOG_WARNING(
                "Texture {} requested {} streamable mips, but the sparse mip tail starts at {}, {} mips will stream",
                handle.get_id(),
                requested_streamable_mips,
                entry.mip_tail_first_mip,
                effective_streamable_mips);
        }
    }

    m_texture_entry_map.emplace(handle, entry);
}

void GpuTexturePool::unregister_texture(TextureAssetHandle handle)
{
    const auto entry_it = m_texture_entry_map.find(handle);
    if (entry_it == m_texture_entry_map.end())
    {
        MIZU_UNREACHABLE("Trying to unregister texture that was not registered: {}", handle.get_id());
        return;
    }

    TextureEntry& entry = entry_it->second;

    PendingRelease release{
        .texture = std::move(entry.texture),
        .pages = std::move(entry.mip_tail_pages),
        .frames = m_description.release_delay_frames,
    };

    for (const std::vector<GpuPage>& pages : entry.mip_pages)
    {
        release.pages.insert(release.pages.end(), pages.begin(), pages.end());
    }

    auto pending_it = m_pending_page_releases.begin();
    while (pending_it != m_pending_page_releases.end())
    {
        if (pending_it->handle != handle)
        {
            pending_it = std::next(pending_it);
            continue;
        }

        release.pages.insert(release.pages.end(), pending_it->pages.begin(), pending_it->pages.end());
        pending_it = m_pending_page_releases.erase(pending_it);
    }

    if (release.texture != nullptr || !release.pages.empty())
    {
        m_pending_releases.push_back(std::move(release));
    }

    release_delayed(std::move(entry.old_texture));

    m_texture_entry_map.erase(entry_it);
}

bool GpuTexturePool::update_residency_state(
    TextureAssetHandle handle,
    const TextureAssetMetadata& metadata,
    uint32_t first_mip)
{
    TextureEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle: {}", handle.get_id());

    /*
     * Things to do:
     * - if mip_delta > 0
     *      - if sparse
     *          allocate necessary pages for new mips
     *          schedule sparse bind operations
     *      - if placed
     *          create new image with new mips
     *          old texture will be used to copy the contents to new image
     * - if mip_delta < 0
     *      - if sparse
     *          get pages to unbind and add them to the delayed release list
     *          this delayed should free the pages, and schedule the unbind operations
     *      - if placed
     *          create new image with new mips
     *          old texture not really needed
     */

    switch (entry->backing)
    {
    case ResourceBacking::Placed:
        return update_placed_residency_state(handle, *entry, metadata, first_mip);
    case ResourceBacking::Sparse:
        return update_sparse_residency_state(handle, *entry, metadata, first_mip);
    }

    MIZU_UNREACHABLE("Invalid resource backing");
    return false;
}

void GpuTexturePool::record_gpu_upload(
    TextureAssetHandle handle,
    const TextureAssetMetadata& metadata,
    const GpuTextureUploadData& upload,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    TextureEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle {}", handle.get_id());

    switch (entry->backing)
    {
    case ResourceBacking::Placed:
        record_placed_gpu_upload(*entry, metadata, upload, command, frame_allocator);
        break;
    case ResourceBacking::Sparse:
        record_sparse_gpu_upload(handle, *entry, metadata, upload, command, frame_allocator);
        break;
    }
}

uint32_t GpuTexturePool::get_coarsest_first_mip(TextureAssetHandle handle) const
{
    const TextureEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle: {}", handle.get_id());

    if (entry->backing != ResourceBacking::Sparse)
        return std::numeric_limits<uint32_t>::max();

    return entry->base_first_mip;
}

ImageResourceView GpuTexturePool::get_resource_view(TextureAssetHandle handle) const
{
    const TextureEntry* entry = get_entry(handle);
    MIZU_ASSERT(entry != nullptr, "Failed to find entry for handle: {}", handle.get_id());
    MIZU_ASSERT(entry->texture != nullptr, "Texture {} has no resident image", handle.get_id());

    if (entry->backing == ResourceBacking::Placed)
        return ImageResourceView::create(entry->texture);

    const uint32_t num_mips = entry->texture->get_description().num_mips;

    return ImageResourceView::create(
        entry->texture,
        ImageResourceViewDescription{
            .mip_base = num_mips - entry->resident_mips,
            .mip_count = entry->resident_mips,
        });
}

bool GpuTexturePool::update_placed_residency_state(
    TextureAssetHandle handle,
    TextureEntry& entry,
    const TextureAssetMetadata& metadata,
    uint32_t first_mip)
{
    const uint32_t resident_first_mip = metadata.num_mips - entry.resident_mips;

    if (resident_first_mip == first_mip)
    {
        MIZU_LOG_ERROR(
            "Trying to update texture with first_mip = {} when that mip is already resident ({})",
            first_mip,
            handle.get_id());
        return false;
    }

    if (first_mip == metadata.num_mips)
    {
        release_delayed(std::move(entry.texture));
        entry.texture = nullptr;
        entry.resident_mips = 0;

        return true;
    }

    const ImageDescription image_desc = create_image_description(handle, metadata, first_mip);

    entry.old_texture = std::move(entry.texture);
    entry.texture = g_render_device->create_image(image_desc);
    entry.resident_mips = metadata.num_mips - first_mip;

    return true;
}

bool GpuTexturePool::update_sparse_residency_state(
    TextureAssetHandle handle,
    TextureEntry& entry,
    const TextureAssetMetadata& metadata,
    uint32_t first_mip)
{
    MIZU_ASSERT(entry.texture != nullptr, "Sparse resources should have the texture created on register");
    MIZU_ASSERT(
        first_mip <= metadata.num_mips, "Invalid first mip {}, texture has {} mips", first_mip, metadata.num_mips);

    MIZU_ASSERT(
        first_mip == metadata.num_mips || first_mip <= entry.base_first_mip,
        "First mip {} is coarser than the base allocation (starts at {})",
        first_mip,
        entry.base_first_mip);

    const uint32_t num_mips = metadata.num_mips;
    const uint32_t old_first_mip = num_mips - entry.resident_mips;

    if (first_mip == old_first_mip)
        return true;

    if (first_mip == num_mips)
    {
        // Eviction case

        for (uint32_t mip = old_first_mip; mip < entry.mip_tail_first_mip; ++mip)
        {
            release_sparse_pages(handle, entry, mip);
        }

        release_sparse_pages(handle, entry, MIP_TAIL);

        entry.resident_mips = 0;
        return true;
    }

    if (first_mip > old_first_mip)
    {
        entry.resident_mips = num_mips - first_mip;
        return true;
    }

    const bool first_load = entry.resident_mips == 0;
    const uint32_t bound_first_mip = first_load ? entry.base_first_mip : old_first_mip;

    std::vector<uint32_t> mips{};

    if (first_load)
    {
        if (entry.texture->get_sparse_memory_requirements().mip_tail_size != 0)
        {
            mips.push_back(MIP_TAIL);
        }

        for (uint32_t mip = entry.base_first_mip; mip < entry.mip_tail_first_mip; ++mip)
        {
            mips.push_back(mip);
        }
    }

    for (uint32_t mip = first_mip; mip < bound_first_mip; ++mip)
    {
        mips.push_back(mip);
    }

    if (!acquire_sparse_pages(handle, entry, mips))
        return false;

    entry.resident_mips = num_mips - first_mip;
    return true;
}

struct MipUploadLayout
{
    glm::uvec2 dims;
    uint32_t row_pitch;
    uint64_t src_offset;
    uint64_t dst_offset;
};

static std::vector<MipUploadLayout> compute_mips_upload_layout(
    const TextureAssetMetadata& metadata,
    uint32_t first_mip,
    uint32_t mip_count,
    uint64_t row_pitch_alignment,
    uint64_t placement_alignment,
    uint64_t& out_total_size)
{
    const uint32_t format_size = get_image_format_size(metadata.format);
    const uint64_t src_base = metadata.get_mip_offset(first_mip);

    std::vector<MipUploadLayout> layouts;
    layouts.reserve(mip_count);

    uint64_t total = 0;
    for (uint32_t i = 0; i < mip_count; ++i)
    {
        const uint32_t mip = first_mip + i;
        const glm::uvec2 dims = compute_mip_size(metadata.width, metadata.height, mip);

        const uint32_t row_pitch = static_cast<uint32_t>(
            math::align_up_pow2(static_cast<uint64_t>(dims.x) * format_size, row_pitch_alignment));
        const uint64_t dst_offset = math::align_up_pow2(total, placement_alignment);

        layouts.push_back({dims, row_pitch, metadata.get_mip_offset(mip) - src_base, dst_offset});
        total = dst_offset + static_cast<uint64_t>(row_pitch) * dims.y * metadata.depth;
    }

    out_total_size = total;

    return layouts;
}

static void record_staged_mips_upload(
    ImageResource& texture,
    const TextureAssetMetadata& metadata,
    const GpuTextureUploadData& upload,
    uint32_t texture_first_mip,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    if (upload.new_first_mip >= upload.old_first_mip)
        return;

    MIZU_ASSERT(!upload.data.empty(), "Upgrade without staged data");

    const DeviceProperties& properties = g_render_device->get_properties();
    const uint64_t row_pitch_alignment = properties.min_texture_row_pitch_alignment;
    const uint64_t placement_alignment = properties.min_texture_data_placement_alignment;

    const uint32_t streamed_count = upload.old_first_mip - upload.new_first_mip;

    uint64_t frame_alloc_size = 0;
    const std::vector<MipUploadLayout> layouts = compute_mips_upload_layout(
        metadata, upload.new_first_mip, streamed_count, row_pitch_alignment, placement_alignment, frame_alloc_size);

    const FrameAllocation allocation = frame_allocator.allocate(frame_alloc_size, placement_alignment, sizeof(uint8_t));

    uint8_t* mapped = allocation.get_mapped_data();
    const uint8_t* src = upload.data.data();
    const uint32_t format_size = get_image_format_size(metadata.format);

    for (uint32_t i = 0; i < streamed_count; ++i)
    {
        const MipUploadLayout& layout = layouts[i];

        const uint64_t src_row_bytes = static_cast<uint64_t>(layout.dims.x) * format_size;
        const uint64_t rows = static_cast<uint64_t>(layout.dims.y) * metadata.depth;

        if (layout.row_pitch == src_row_bytes)
        {
            memcpy(mapped + layout.dst_offset, src + layout.src_offset, src_row_bytes * rows);
        }
        else
        {
            for (uint64_t row = 0; row < rows; ++row)
            {
                memcpy(
                    mapped + layout.dst_offset + row * layout.row_pitch,
                    src + layout.src_offset + row * src_row_bytes,
                    src_row_bytes);
            }
        }

        const CopyBufferToImageInfo info{
            .buffer_offset = allocation.view.desc.offset + layout.dst_offset,
            .buffer_row_length = layout.row_pitch / format_size,
            .image_subresource_layers =
                {
                    .mip_level = upload.new_first_mip + i - texture_first_mip,
                    .base_array_layer = 0,
                    .layer_count = 1,
                },
            .image_offset = {0, 0, 0},
            .image_extent = {layout.dims.x, layout.dims.y, metadata.depth},
        };

        command.copy_buffer_to_image(*frame_allocator.get_buffer(), texture, info);
    }
}

void GpuTexturePool::record_placed_gpu_upload(
    TextureEntry& entry,
    const TextureAssetMetadata& metadata,
    const GpuTextureUploadData& upload,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(entry.texture != nullptr, "Recording upload of placed texture without a texture");

    const uint32_t num_mips = metadata.num_mips;

    ImageResource* new_texture = entry.texture.get();
    ImageResource* old_texture = entry.old_texture.get();

    command.transition_resource(*new_texture, ImageResourceState::Undefined, ImageResourceState::TransferDst);

    if (old_texture != nullptr)
    {
        command.transition_resource(*old_texture, ImageResourceState::ShaderReadOnly, ImageResourceState::TransferSrc);

        const uint32_t shared_first_mip = std::max(upload.old_first_mip, upload.new_first_mip);
        for (uint32_t mip = shared_first_mip; mip < num_mips; ++mip)
        {
            const glm::uvec2 dims = compute_mip_size(metadata.width, metadata.height, mip);

            const CopyImageToImageInfo info{
                .source_offset = {0, 0, 0},
                .source_subresource_layers =
                    {
                        .mip_level = mip - upload.old_first_mip,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                .dest_offset = {0, 0, 0},
                .dest_subresource_layers =
                    {
                        .mip_level = mip - upload.new_first_mip,
                        .base_array_layer = 0,
                        .layer_count = 1,
                    },
                .extent = {dims.x, dims.y, metadata.depth},
            };

            command.copy_image_to_image(*old_texture, *new_texture, info);
        }
    }

    record_staged_mips_upload(*new_texture, metadata, upload, upload.new_first_mip, command, frame_allocator);

    command.transition_resource(*new_texture, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);

    if (old_texture != nullptr)
    {
        command.transition_resource(*old_texture, ImageResourceState::TransferSrc, ImageResourceState::ShaderReadOnly);
    }

    release_delayed(std::move(entry.old_texture));
    entry.old_texture = nullptr;
}

void GpuTexturePool::record_sparse_gpu_upload(
    TextureAssetHandle handle,
    TextureEntry& entry,
    const TextureAssetMetadata& metadata,
    const GpuTextureUploadData& upload,
    CommandBuffer& command,
    FrameLinearAllocator& frame_allocator)
{
    MIZU_ASSERT(entry.texture != nullptr, "Sparse resources should have the texture created on register");

    if (upload.new_first_mip > upload.old_first_mip)
    {
        for (uint32_t mip = upload.old_first_mip; mip < upload.new_first_mip; ++mip)
        {
            release_sparse_pages(handle, entry, mip);
        }

        return;
    }

    if (upload.new_first_mip == upload.old_first_mip)
        return;

    ImageResource& texture = *entry.texture;

    const ImageResourceViewDescription upload_range{
        .mip_base = upload.new_first_mip,
        .mip_count = upload.old_first_mip - upload.new_first_mip,
    };

    command.transition_resource(texture, ImageResourceState::Undefined, ImageResourceState::TransferDst, upload_range);
    record_staged_mips_upload(texture, metadata, upload, 0, command, frame_allocator);
    command.transition_resource(
        texture, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly, upload_range);
}

static bool are_pages_contiguous(const GpuPage& first, const GpuPage& second, uint64_t page_size)
{
    return first.placement.heap == second.placement.heap
           && second.placement.offset == first.placement.offset + page_size;
}

static size_t count_contiguous_pages(std::span<const GpuPage> pages, uint64_t page_size)
{
    size_t count = 1;
    while (count < pages.size())
    {
        if (!are_pages_contiguous(pages[count - 1], pages[count], page_size))
            break;

        count += 1;
    }

    return count;
}

static glm::uvec3 get_mip_extent(const ImageDescription& desc, uint32_t mip)
{
    const glm::uvec2 size = compute_mip_size(desc.width, desc.height, mip);
    const uint32_t depth = desc.type == ImageType::Image3D ? glm::max(1u, desc.depth >> mip) : 1u;

    return glm::uvec3(size, depth);
}

std::vector<GpuPage>& GpuTexturePool::get_sparse_pages(TextureEntry& entry, uint32_t mip)
{
    if (mip == MIP_TAIL)
        return entry.mip_tail_pages;

    MIZU_ASSERT(mip < entry.mip_tail_first_mip, "Mip {} is part of the mip tail", mip);
    return entry.mip_pages[mip];
}

uint32_t GpuTexturePool::count_sparse_pages(const TextureEntry& entry, uint32_t mip) const
{
    const SparseImageMemoryRequirements reqs = entry.texture->get_sparse_memory_requirements();

    if (mip == MIP_TAIL)
        return static_cast<uint32_t>(math::ceil_div(reqs.mip_tail_size, reqs.page_size));

    const glm::uvec3 tiles = get_sparse_mip_tile_count(entry.texture->get_description(), reqs, mip);
    return tiles.x * tiles.y * tiles.z;
}

bool GpuTexturePool::acquire_sparse_pages(
    TextureAssetHandle handle,
    TextureEntry& entry,
    std::span<const uint32_t> mips)
{
    const auto find_pending_release = [&](uint32_t mip) {
        return std::ranges::find_if(m_pending_page_releases, [&](const PendingPageRelease& release) {
            return release.handle == handle && release.mip == mip;
        });
    };

    uint32_t num_new_pages = 0;
    for (const uint32_t mip : mips)
    {
        MIZU_ASSERT(
            get_sparse_pages(entry, mip).empty(), "Sparse mip {} of texture {} is already bound", mip, handle.get_id());

        if (find_pending_release(mip) == m_pending_page_releases.end())
        {
            num_new_pages += count_sparse_pages(entry, mip);
        }
    }

    std::vector<GpuPage> new_pages{};
    if (num_new_pages != 0)
    {
        if (!m_page_allocator.allocate_pages(num_new_pages, new_pages))
        {
            MIZU_LOG_WARNING("Failed to allocate {} sparse pages for texture {}", num_new_pages, handle.get_id());
            return false;
        }
    }

    size_t next_page = 0;
    for (const uint32_t mip : mips)
    {
        std::vector<GpuPage>& pages = get_sparse_pages(entry, mip);

        const auto pending_it = find_pending_release(mip);
        if (pending_it != m_pending_page_releases.end())
        {
            pages = std::move(pending_it->pages);
            m_pending_page_releases.erase(pending_it);
            continue;
        }

        const uint32_t count = count_sparse_pages(entry, mip);

        const std::span<const GpuPage> mip_new_pages = std::span(new_pages).subspan(next_page, count);
        pages.assign(mip_new_pages.begin(), mip_new_pages.end());

        next_page += count;

        enqueue_sparse_binds(entry, mip);
    }

    return true;
}

void GpuTexturePool::release_sparse_pages(TextureAssetHandle handle, TextureEntry& entry, uint32_t mip)
{
    std::vector<GpuPage>& pages = get_sparse_pages(entry, mip);
    if (pages.empty())
        return;

    m_pending_page_releases.push_back({
        .handle = handle,
        .mip = mip,
        .pages = std::move(pages),
        .frames = m_description.release_delay_frames,
    });

    pages.clear();
}

void GpuTexturePool::enqueue_sparse_binds(const TextureEntry& entry, uint32_t mip)
{
    ImageResource* texture = entry.texture.get();
    const SparseImageMemoryRequirements reqs = texture->get_sparse_memory_requirements();

    if (mip == MIP_TAIL)
    {
        const std::span<const GpuPage> pages = entry.mip_tail_pages;

        size_t first = 0;
        while (first < pages.size())
        {
            const size_t run = count_contiguous_pages(pages.subspan(first), reqs.page_size);
            const uint64_t tail_offset = first * reqs.page_size;

            m_sparse_bind_scheduler.enqueue_mip_tail_bind(
                SparseMipTailBind{
                    .image = texture,
                    .array_layer = 0,
                    .tail_offset = tail_offset,
                    .size = std::min(run * reqs.page_size, reqs.mip_tail_size - tail_offset),
                    .memory = pages[first].placement,
                });

            first += run;
        }

        return;
    }

    const ImageDescription& desc = texture->get_description();
    const glm::uvec3 tiles = get_sparse_mip_tile_count(desc, reqs, mip);
    const glm::uvec3 mip_extent = get_mip_extent(desc, mip);

    const std::span<const GpuPage> pages = entry.mip_pages[mip];
    MIZU_ASSERT(
        pages.size() == static_cast<size_t>(tiles.x) * tiles.y * tiles.z,
        "Sparse mip {} has {} pages, expected {}",
        mip,
        pages.size(),
        tiles.x * tiles.y * tiles.z);

    for (uint32_t z = 0; z < tiles.z; ++z)
    {
        for (uint32_t y = 0; y < tiles.y; ++y)
        {
            const size_t row_first = (static_cast<size_t>(z) * tiles.y + y) * tiles.x;

            uint32_t x = 0;
            while (x < tiles.x)
            {
                const std::span<const GpuPage> row_pages = pages.subspan(row_first + x, tiles.x - x);
                const uint32_t run = static_cast<uint32_t>(count_contiguous_pages(row_pages, reqs.page_size));

                const glm::uvec3 offset = glm::uvec3(x, y, z) * reqs.tile_extent;
                const glm::uvec3 extent = glm::min(glm::uvec3(run, 1, 1) * reqs.tile_extent, mip_extent - offset);

                m_sparse_bind_scheduler.enqueue_image_bind(
                    SparseImageBind{
                        .image = texture,
                        .mip_level = mip,
                        .array_layer = 0,
                        .offset = offset,
                        .extent = extent,
                        .memory = row_pages[0].placement,
                    });

                x += run;
            }
        }
    }
}

void GpuTexturePool::enqueue_sparse_unbinds(ImageResource& texture, uint32_t mip)
{
    if (mip == MIP_TAIL)
    {
        m_sparse_bind_scheduler.add_mip_tail_unbind(SparseMipTailUnbind{.image = &texture, .array_layer = 0});
        return;
    }

    m_sparse_bind_scheduler.enqueue_image_unbind(
        SparseImageUnbind{
            .image = &texture,
            .mip_level = mip,
            .array_layer = 0,
            .offset = glm::uvec3(0),
            .extent = get_mip_extent(texture.get_description(), mip),
        });
}

void GpuTexturePool::release_delayed(std::shared_ptr<ImageResource> texture)
{
    if (texture == nullptr)
        return;

    m_pending_releases.push_back({
        .texture = std::move(texture),
        .frames = m_description.release_delay_frames,
    });
}

void GpuTexturePool::process_pending_page_releases()
{
    auto it = m_pending_page_releases.begin();
    while (it != m_pending_page_releases.end())
    {
        it->frames -= 1;

        if (it->frames != 0)
        {
            it = std::next(it);
            continue;
        }

        const TextureEntry* entry = get_entry(it->handle);
        if (entry != nullptr && entry->texture != nullptr)
        {
            enqueue_sparse_unbinds(*entry->texture, it->mip);
        }

        m_page_allocator.free_pages(it->pages);
        it = m_pending_page_releases.erase(it);
    }
}

ImageDescription GpuTexturePool::create_image_description(
    TextureAssetHandle handle,
    const TextureAssetMetadata& metadata,
    uint32_t first_mip)
{
    const glm::uvec2 mip_dims = compute_mip_size(metadata.width, metadata.height, first_mip);

    ImageDescription desc{};
    desc.width = mip_dims.x;
    desc.height = mip_dims.y;
    desc.depth = metadata.depth;
    desc.type = ImageType::Image2D;
    desc.format = metadata.format;
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst | ImageUsageBits::TransferSrc;
    desc.num_mips = metadata.num_mips - first_mip;
    desc.name = std::format("TextureStreamingManagerTexture_{}", handle.get_id());

    return desc;
}

ResourceBacking GpuTexturePool::select_resource_backing(TextureAssetHandle handle, const TextureAssetMetadata& metadata)
{
    if (!s_sparse_resources_supported)
    {
        return ResourceBacking::Placed;
    }

    if (metadata.streamable_mips == 0)
    {
        return ResourceBacking::Placed;
    }

    const ImageDescription dummy_desc = create_image_description(handle, metadata, 0);
    const auto dummy_texture = g_render_device->create_sparse_image(dummy_desc);

    const SparseImageMemoryRequirements sparse_reqs = dummy_texture->get_sparse_memory_requirements();

    // The page size of the sparse memory requirements must match the value of the page allocator
    if (sparse_reqs.page_size != m_page_allocator.get_page_size())
    {
        return ResourceBacking::Placed;
    }

    if (sparse_reqs.metadata_size != 0)
    {
        return ResourceBacking::Placed;
    }

    const uint32_t num_streamable_mips = math::min(
        static_cast<uint32_t>(metadata.streamable_mips), metadata.num_mips - 1, sparse_reqs.mip_tail_first_mip);

    // This means that all of the mips of the texture are tail mips, can't stream
    if (num_streamable_mips == 0)
    {
        return ResourceBacking::Placed;
    }

    return ResourceBacking::Sparse;
}

auto GpuTexturePool::get_entry(TextureAssetHandle handle) -> TextureEntry*
{
    const auto it = m_texture_entry_map.find(handle);
    if (it == m_texture_entry_map.end())
        return nullptr;

    return &it->second;
}

auto GpuTexturePool::get_entry(TextureAssetHandle handle) const -> const TextureEntry*
{
    const auto it = m_texture_entry_map.find(handle);
    if (it == m_texture_entry_map.end())
        return nullptr;

    return &it->second;
}

} // namespace Mizu
