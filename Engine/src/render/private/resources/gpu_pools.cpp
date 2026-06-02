#include "resources/gpu_pools.h"

#include <algorithm>
#include <string>

#include "asset/asset.h"
#include "asset/asset_loader.h"
#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "render/runtime/renderer.h"

namespace Mizu
{

static uint64_t gpu_pool_align_up(uint64_t value, uint64_t alignment)
{
    return ((value + alignment - 1) / alignment) * alignment;
}

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
        const uint64_t aligned_offset = gpu_pool_align_up(range.offset, alignment);
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

    return GpuMeshAllocationHandle{handle, *vertex_offset, vertex_size, *index_offset, index_size};
}

void GpuMeshPool::free(const GpuMeshAllocationHandle& allocation)
{
    if (allocation.vertex_size != 0)
        m_vertex_allocator.free(allocation.vertex_offset, allocation.vertex_size);

    if (allocation.index_size != 0)
        m_index_allocator.free(allocation.index_offset, allocation.index_size);
}

//
// GpuTexturePool
//

bool GpuTexturePool::init(uint64_t size)
{
    (void)size;
    std::lock_guard lock{m_mutex};
    m_images.clear();
    return true;
}

std::optional<GpuTextureAllocationHandle> GpuTexturePool::allocate(
    const TextureAssetHandle& handle,
    const TexturePayload& payload)
{
    MIZU_ASSERT(handle.is_valid(), "Trying to allocate invalid TextureAssetHandle from GpuTexturePool");
    MIZU_ASSERT(
        payload.width > 0 && payload.height > 0 && payload.depth > 0,
        "Trying to allocate texture with invalid dimensions: {}x{}x{}",
        payload.width,
        payload.height,
        payload.depth);

    ImageDescription desc{};
    desc.width = payload.width;
    desc.height = payload.height;
    desc.depth = payload.depth;
    desc.type = ImageType::Image2D;
    desc.format = payload.format;
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = static_cast<uint32_t>(std::max<uint64_t>(1, payload.num_mips));
    desc.num_layers = 1;
    desc.name = std::string{"GpuTexturePool_Texture_"} + std::to_string(handle.get_id());

    const std::shared_ptr<ImageResource> image = g_render_device->create_image(desc);
    if (image == nullptr)
    {
        MIZU_LOG_ERROR("Failed to create image for texture handle: {}", handle.get_id());
        return std::nullopt;
    }

    {
        std::lock_guard lock{m_mutex};
        m_images[handle] = image;
    }

    return GpuTextureAllocationHandle{handle};
}

std::shared_ptr<ImageResource> GpuTexturePool::get_image(const GpuTextureAllocationHandle& allocation) const
{
    return get_image(allocation.handle);
}

std::shared_ptr<ImageResource> GpuTexturePool::get_image(const TextureAssetHandle& handle) const
{
    std::lock_guard lock{m_mutex};

    const auto image_it = m_images.find(handle);
    if (image_it == m_images.end())
        return nullptr;

    return image_it->second;
}

void GpuTexturePool::free(const GpuTextureAllocationHandle& allocation)
{
    if (!allocation.handle.is_valid())
        return;

    std::lock_guard lock{m_mutex};
    m_images.erase(allocation.handle);
}

} // namespace Mizu
