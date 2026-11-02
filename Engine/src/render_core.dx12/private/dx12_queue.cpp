#include "dx12_queue.h"

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "base/math/math.h"

#include "dx12_command_buffer.h"
#include "dx12_context.h"
#include "dx12_synchronization.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

static ID3D12Fence* get_native_fence(const SyncPoint& point)
{
    return static_cast<const Dx12Fence*>(point.fence)->handle();
}

Dx12Queue::Dx12Queue(ID3D12CommandQueue* queue, CommandBufferType type) : m_queue(queue), m_type(type)
{
    m_fence = std::make_unique<Dx12Fence>(0);
}

Dx12Queue::~Dx12Queue()
{
    m_queue->Release();
}

SyncPoint Dx12Queue::submit(std::span<CommandBuffer*> command_buffers, const SubmitInfo& info)
{
    MIZU_PROFILE_SCOPED;

    constexpr size_t MAX_COMMAND_LISTS = 16;
    MIZU_ASSERT(
        command_buffers.size() <= MAX_COMMAND_LISTS,
        "Too many command buffers in a single submit ({} > {})",
        command_buffers.size(),
        MAX_COMMAND_LISTS);

    inplace_vector<ID3D12CommandList*, MAX_COMMAND_LISTS> command_lists;
    for (const CommandBuffer* command_buffer : command_buffers)
    {
        MIZU_ASSERT(
            &Dx12Context.device->get_native_queue(command_buffer->get_type()) == this,
            "Command buffer can't be submitted to this queue");

        command_lists.push_back(static_cast<const Dx12CommandBuffer*>(command_buffer)->handle());
    }

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        if (wait.is_valid())
            DX12_CHECK(m_queue->Wait(get_native_fence(wait), wait.value));
    }

    if (!command_lists.empty())
        m_queue->ExecuteCommandLists(static_cast<UINT>(command_lists.size()), command_lists.data());

    m_last_value += 1;
    DX12_CHECK(m_queue->Signal(m_fence->handle(), m_last_value));

    for (const SyncPoint& signal : info.signals)
        DX12_CHECK(m_queue->Signal(get_native_fence(signal), signal.value));

    return SyncPoint{m_fence.get(), m_last_value};
}

static void update_buffer_tile_mapping(ID3D12CommandQueue* queue, const SparseBufferMapping& mapping)
{
    const Dx12BufferResource& native_buffer = static_cast<const Dx12BufferResource&>(*mapping.buffer);
    const ID3D12Resource* resource = native_buffer.handle();

    MIZU_ASSERT(
        mapping.resource_offset % D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES == 0,
        "Sparse buffer resource offset must be tile aligned");
    MIZU_ASSERT(
        mapping.size != 0 && mapping.size % D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES == 0,
        "Sparse buffer mapping size must be a non-zero multiple of the tile size");
    MIZU_ASSERT(
        mapping.resource_offset / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES <= std::numeric_limits<UINT>::max(),
        "Sparse buffer offset exceeds D3D12 tile coordinate range");

    const UINT tile_count = static_cast<UINT>(mapping.size / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
    MIZU_ASSERT(tile_count != 0, "Invalid sparse buffer tile count");

    // TODO: implement unmapping support for sparse buffers
    bool unmapping = false;
    if (!unmapping)
    {
        MIZU_ASSERT(
            mapping.memory.offset % D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES == 0, "Heap offset must be tile aligned");
        MIZU_ASSERT(mapping.memory.size >= mapping.size, "Heap allocation is too small for buffer mapping");
    }

    D3D12_TILED_RESOURCE_COORDINATE coordinate{};
    coordinate.X = static_cast<UINT>(mapping.resource_offset / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
    coordinate.Y = 0;
    coordinate.Z = 0;
    coordinate.Subresource = 0;

    D3D12_TILE_REGION_SIZE region{};
    region.NumTiles = tile_count;
    region.UseBox = FALSE;
    region.Width = 0;
    region.Height = 0;
    region.Depth = 0;

    const Dx12GpuHeap& native_heap = static_cast<const Dx12GpuHeap&>(*mapping.memory.heap);

    const D3D12_TILE_RANGE_FLAGS range_flag = unmapping ? D3D12_TILE_RANGE_FLAG_NULL : D3D12_TILE_RANGE_FLAG_NONE;
    const UINT heap_offset =
        unmapping ? 0 : static_cast<UINT>(mapping.memory.offset / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
    const UINT* heap_offset_ptr = unmapping ? nullptr : &heap_offset;

    queue->UpdateTileMappings(
        const_cast<ID3D12Resource*>(resource),
        1,
        &coordinate,
        &region,
        native_heap.handle(),
        1,
        &range_flag,
        heap_offset_ptr,
        &tile_count,
        D3D12_TILE_MAPPING_FLAG_NONE);
}

static void update_image_tile_mapping(ID3D12CommandQueue* queue, const SparseImageMapping& mapping)
{
    const Dx12ImageResource& native_image = static_cast<const Dx12ImageResource&>(*mapping.image);
    ID3D12Resource* resource = native_image.handle();

    const D3D12_RESOURCE_DESC1 desc = get_dx12_resource_desc(native_image.get_description());
    MIZU_ASSERT(mapping.mip_level < desc.MipLevels, "Invalid sparse image mip level");

    const UINT array_size = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;
    MIZU_ASSERT(mapping.array_layer < array_size, "Invalid sparse image array layer");

    // TODO: implement correct plane slice, assuming color and depth
    const UINT plane_slice = 0;
    const UINT subresource =
        calc_dx12_subresource_index(mapping.mip_level, mapping.array_layer, plane_slice, desc.MipLevels, array_size);

    UINT num_subresource_tilings = 1;
    D3D12_TILE_SHAPE tile_shape{};
    D3D12_SUBRESOURCE_TILING subresource_tiling{};

    Dx12Context.device->handle()->GetResourceTiling(
        resource, nullptr, nullptr, &tile_shape, &num_subresource_tilings, subresource, &subresource_tiling);

    MIZU_ASSERT(num_subresource_tilings == 1, "Failed to retrieve D3D12 subresource tiling");

    // Packed mips do not have standard rectangular tile layouts.
    MIZU_ASSERT(
        subresource_tiling.StartTileIndexInOverallResource != D3D12_PACKED_TILE,
        "Packed mip mappings require a separate implementation");
    MIZU_ASSERT(
        tile_shape.WidthInTexels != 0 && tile_shape.HeightInTexels != 0 && tile_shape.DepthInTexels != 0,
        "Invalid D3D12 tile shape");

    // Calculate the actual texel dimensions of this mip.
    const UINT mip_width = static_cast<UINT>(std::max<uint64_t>(1, desc.Width >> mapping.mip_level));
    const UINT mip_height =
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ? 1 : std::max<UINT>(1, desc.Height >> mapping.mip_level);
    const UINT mip_depth = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
                               ? std::max<UINT>(1, static_cast<UINT>(desc.DepthOrArraySize >> mapping.mip_level))
                               : 1;

    const UINT offset_x = mapping.offset.x;
    const UINT offset_y = mapping.offset.y;
    const UINT offset_z = mapping.offset.z;

    const UINT extent_x = mapping.extent.x;
    const UINT extent_y = mapping.extent.y;
    const UINT extent_z = mapping.extent.z;

    MIZU_ASSERT(extent_x != 0 && extent_y != 0 && extent_z != 0, "Sparse image mapping extent cannot be zero");
    MIZU_ASSERT(
        offset_x + extent_x <= mip_width && offset_y + extent_y <= mip_height && offset_z + extent_z <= mip_depth,
        "Sparse image mapping exceeds mip dimensions");

    // Region offsets must start on tile boundaries.
    MIZU_ASSERT(
        offset_x % tile_shape.WidthInTexels == 0 && offset_y % tile_shape.HeightInTexels == 0
            && offset_z % tile_shape.DepthInTexels == 0,
        "Sparse image mapping offset must be tile aligned");

    // Partial extents are only valid at the corresponding mip edge.
    MIZU_ASSERT(
        extent_x % tile_shape.WidthInTexels == 0 || offset_x + extent_x == mip_width,
        "Sparse image mapping width must be tile aligned unless it reaches the mip edge");
    MIZU_ASSERT(
        extent_y % tile_shape.HeightInTexels == 0 || offset_y + extent_y == mip_height,
        "Sparse image mapping height must be tile aligned unless it reaches the mip edge");
    MIZU_ASSERT(
        extent_z % tile_shape.DepthInTexels == 0 || offset_z + extent_z == mip_depth,
        "Sparse image mapping depth must be tile aligned unless it reaches the mip edge");

    const UINT tile_x = offset_x / tile_shape.WidthInTexels;
    const UINT tile_y = offset_y / tile_shape.HeightInTexels;
    const UINT tile_z = offset_z / tile_shape.DepthInTexels;

    const UINT tile_width = math::ceil_div(extent_x, tile_shape.WidthInTexels);
    const UINT tile_height = math::ceil_div(extent_y, tile_shape.HeightInTexels);
    const UINT tile_depth = math::ceil_div(extent_z, tile_shape.DepthInTexels);

    MIZU_ASSERT(
        tile_x + tile_width <= subresource_tiling.WidthInTiles
            && tile_y + tile_height <= subresource_tiling.HeightInTiles
            && tile_z + tile_depth <= subresource_tiling.DepthInTiles,
        "Sparse image mapping exceeds subresource tile dimensions");

    const uint64_t tile_count_64 = static_cast<uint64_t>(tile_width) * tile_height * tile_depth;
    MIZU_ASSERT(tile_count_64 <= std::numeric_limits<UINT>::max(), "Sparse image mapping contains too many tiles");

    const UINT tile_count = static_cast<UINT>(tile_count_64);

    // TODO: implement unmapping support for sparse images
    const bool unmapping = false;
    if (!unmapping)
    {
        MIZU_ASSERT(
            mapping.memory.offset % D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES == 0, "Heap offset must be tile aligned");
        MIZU_ASSERT(
            mapping.memory.size >= tile_count_64 * D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES,
            "Heap allocation is too small for image mapping");
    }

    D3D12_TILED_RESOURCE_COORDINATE coordinate{};
    coordinate.X = tile_x;
    coordinate.Y = tile_y;
    coordinate.Z = tile_z;
    coordinate.Subresource = subresource;

    D3D12_TILE_REGION_SIZE region{};
    region.NumTiles = tile_count;
    region.UseBox = TRUE;
    region.Width = tile_width;
    region.Height = static_cast<UINT16>(tile_height);
    region.Depth = static_cast<UINT16>(tile_depth);

    MIZU_ASSERT(
        tile_height <= std::numeric_limits<UINT16>::max() && tile_depth <= std::numeric_limits<UINT16>::max(),
        "Sparse image tile region exceeds D3D12 limits");

    const Dx12GpuHeap& native_heap = static_cast<const Dx12GpuHeap&>(*mapping.memory.heap);

    const D3D12_TILE_RANGE_FLAGS range_flag = unmapping ? D3D12_TILE_RANGE_FLAG_NULL : D3D12_TILE_RANGE_FLAG_NONE;
    const UINT heap_offset =
        unmapping ? 0 : static_cast<UINT>(mapping.memory.offset / D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES);
    const UINT* heap_offset_ptr = unmapping ? nullptr : &heap_offset;

    queue->UpdateTileMappings(
        resource,
        1,
        &coordinate,
        &region,
        native_heap.handle(),
        1,
        &range_flag,
        heap_offset_ptr,
        &tile_count,
        D3D12_TILE_MAPPING_FLAG_NONE);
}

SyncPoint Dx12Queue::bind_sparse(const SparseBindBatch& batch, const SubmitInfo& info)
{
    const std::span<const SparseBufferMapping> buffer_mappings = batch.get_buffer_mappings();
    const std::span<const SparseImageMapping> image_mappings = batch.get_image_mappings();

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        DX12_CHECK(m_queue->Wait(get_native_fence(wait), wait.value));
    }

    for (const SparseBufferMapping& mapping : buffer_mappings)
    {
        update_buffer_tile_mapping(m_queue, mapping);
    }

    for (const SparseImageMapping& mapping : image_mappings)
    {
        update_image_tile_mapping(m_queue, mapping);
    }

    for (const SyncPoint& signal : info.signals)
    {
        DX12_CHECK(m_queue->Signal(get_native_fence(signal), signal.value));
    }

    ++m_last_value;
    DX12_CHECK(m_queue->Signal(m_fence->handle(), m_last_value));

    return SyncPoint{m_fence.get(), m_last_value};
}

void Dx12Queue::wait(const SyncPoint& point)
{
    if (!point.is_valid())
        return;

    const std::lock_guard lock(m_mutex);
    DX12_CHECK(m_queue->Wait(get_native_fence(point), point.value));
}

SyncPoint Dx12Queue::get_last_submitted() const
{
    const std::lock_guard lock(m_mutex);
    return SyncPoint{m_fence.get(), m_last_value};
}

void Dx12Queue::wait_idle()
{
    MIZU_PROFILE_SCOPED;

    uint64_t value = 0;
    {
        const std::lock_guard lock(m_mutex);

        value = ++m_last_value;
        DX12_CHECK(m_queue->Signal(m_fence->handle(), value));
    }

    m_fence->wait(value);
}

} // namespace Mizu::Dx12
