#include "dx12_queue.h"

#include <glm/glm.hpp>
#include <limits>

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

namespace
{

constexpr uint64_t DX12_TILE_SIZE = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;

struct Dx12TileRegion
{
    D3D12_TILED_RESOURCE_COORDINATE coordinate{};
    D3D12_TILE_REGION_SIZE size{};
};

UINT to_dx12_tile_index(uint64_t offset)
{
    MIZU_ASSERT(offset % DX12_TILE_SIZE == 0, "Sparse offset must be tile aligned");
    MIZU_ASSERT(offset / DX12_TILE_SIZE <= std::numeric_limits<UINT>::max(), "Sparse offset exceeds D3D12 tile range");

    return static_cast<UINT>(offset / DX12_TILE_SIZE);
}

UINT to_dx12_tile_count(uint64_t size)
{
    // The last tile can be partial when the range reaches the end of the resource
    const uint64_t tile_count = math::ceil_div(size, DX12_TILE_SIZE);
    MIZU_ASSERT(tile_count != 0, "Sparse range can't be empty");
    MIZU_ASSERT(tile_count <= std::numeric_limits<UINT>::max(), "Sparse range contains too many tiles");

    return static_cast<UINT>(tile_count);
}

Dx12TileRegion get_dx12_buffer_tile_region(uint64_t resource_offset, uint64_t size)
{
    Dx12TileRegion region{};
    region.coordinate.X = to_dx12_tile_index(resource_offset);
    region.size.NumTiles = to_dx12_tile_count(size);
    region.size.UseBox = FALSE;

    return region;
}

struct Dx12SparseImageBox
{
    uint32_t mip_level = 0;
    uint32_t array_layer = 0;
    glm::uvec3 offset{};
    glm::uvec3 extent{};
};

Dx12TileRegion get_dx12_image_tile_region(const Dx12ImageResource& image, const Dx12SparseImageBox& box)
{
    const D3D12_RESOURCE_DESC1 desc = get_dx12_resource_desc(image.get_description());
    MIZU_ASSERT(box.mip_level < desc.MipLevels, "Invalid sparse image mip level");

    const UINT array_size = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;
    MIZU_ASSERT(box.array_layer < array_size, "Invalid sparse image array layer");

    // TODO: implement correct plane slice, assuming color and depth
    const UINT plane_slice = 0;
    const UINT subresource =
        calc_dx12_subresource_index(box.mip_level, box.array_layer, plane_slice, desc.MipLevels, array_size);

    const D3D12_TILE_SHAPE& tile_shape = image.get_tile_shape();
    const D3D12_SUBRESOURCE_TILING& subresource_tiling = image.get_subresource_tiling(subresource);

    MIZU_ASSERT(
        tile_shape.WidthInTexels != 0 && tile_shape.HeightInTexels != 0 && tile_shape.DepthInTexels != 0,
        "Invalid D3D12 tile shape");

    // Alignment and bounds validated by SparseBindBatch
    const UINT tile_x = box.offset.x / tile_shape.WidthInTexels;
    const UINT tile_y = box.offset.y / tile_shape.HeightInTexels;
    const UINT tile_z = box.offset.z / tile_shape.DepthInTexels;

    const UINT tile_width = math::ceil_div(box.extent.x, tile_shape.WidthInTexels);
    const UINT tile_height = math::ceil_div(box.extent.y, tile_shape.HeightInTexels);
    const UINT tile_depth = math::ceil_div(box.extent.z, tile_shape.DepthInTexels);

    MIZU_ASSERT(
        tile_x + tile_width <= subresource_tiling.WidthInTiles
            && tile_y + tile_height <= subresource_tiling.HeightInTiles
            && tile_z + tile_depth <= subresource_tiling.DepthInTiles,
        "Sparse image box exceeds subresource tile dimensions");
    MIZU_ASSERT(
        tile_height <= std::numeric_limits<UINT16>::max() && tile_depth <= std::numeric_limits<UINT16>::max(),
        "Sparse image tile region exceeds D3D12 limits");

    const uint64_t tile_count = static_cast<uint64_t>(tile_width) * tile_height * tile_depth;
    MIZU_ASSERT(tile_count <= std::numeric_limits<UINT>::max(), "Sparse image box contains too many tiles");

    Dx12TileRegion region{};
    region.coordinate.X = tile_x;
    region.coordinate.Y = tile_y;
    region.coordinate.Z = tile_z;
    region.coordinate.Subresource = subresource;
    region.size.NumTiles = static_cast<UINT>(tile_count);
    region.size.UseBox = TRUE;
    region.size.Width = tile_width;
    region.size.Height = static_cast<UINT16>(tile_height);
    region.size.Depth = static_cast<UINT16>(tile_depth);

    return region;
}

Dx12TileRegion get_dx12_mip_tail_tile_region(
    const Dx12ImageResource& image,
    uint32_t array_layer,
    uint64_t tail_offset,
    uint64_t size)
{
    const D3D12_RESOURCE_DESC1 desc = get_dx12_resource_desc(image.get_description());
    const UINT array_size = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : desc.DepthOrArraySize;

    const D3D12_PACKED_MIP_INFO& packed_mip_info = image.get_packed_mip_info();
    MIZU_ASSERT(packed_mip_info.NumPackedMips > 0, "Image has no packed mips");

    // Packed mips are addressed by the first packed subresource, X is the tile index inside the packed tiles
    const UINT plane_slice = 0;
    const UINT subresource = calc_dx12_subresource_index(
        packed_mip_info.NumStandardMips, array_layer, plane_slice, desc.MipLevels, array_size);

    Dx12TileRegion region{};
    region.coordinate.X = to_dx12_tile_index(tail_offset);
    region.coordinate.Subresource = subresource;
    region.size.NumTiles = to_dx12_tile_count(size);
    region.size.UseBox = FALSE;

    MIZU_ASSERT(
        region.coordinate.X + region.size.NumTiles <= packed_mip_info.NumTilesForPackedMips,
        "Sparse mip tail range exceeds packed mip tiles");

    return region;
}

Dx12TileRegion to_dx12_tile_region(const SparseBufferBind& bind)
{
    return get_dx12_buffer_tile_region(bind.resource_offset, bind.size);
}

Dx12TileRegion to_dx12_tile_region(const SparseBufferUnbind& unbind)
{
    return get_dx12_buffer_tile_region(unbind.resource_offset, unbind.size);
}

Dx12TileRegion to_dx12_tile_region(const SparseImageBind& bind)
{
    return get_dx12_image_tile_region(
        static_cast<const Dx12ImageResource&>(*bind.image),
        Dx12SparseImageBox{
            .mip_level = bind.mip_level,
            .array_layer = bind.array_layer,
            .offset = bind.offset,
            .extent = bind.extent,
        });
}

Dx12TileRegion to_dx12_tile_region(const SparseImageUnbind& unbind)
{
    return get_dx12_image_tile_region(
        static_cast<const Dx12ImageResource&>(*unbind.image),
        Dx12SparseImageBox{
            .mip_level = unbind.mip_level,
            .array_layer = unbind.array_layer,
            .offset = unbind.offset,
            .extent = unbind.extent,
        });
}

Dx12TileRegion to_dx12_tile_region(const SparseMipTailBind& bind)
{
    const Dx12ImageResource& native_image = static_cast<const Dx12ImageResource&>(*bind.image);
    return get_dx12_mip_tail_tile_region(
        native_image, bind.array_layer, 0, native_image.get_sparse_memory_requirements().mip_tail_size);
}

Dx12TileRegion to_dx12_tile_region(const SparseMipTailUnbind& unbind)
{
    const Dx12ImageResource& native_image = static_cast<const Dx12ImageResource&>(*unbind.image);
    return get_dx12_mip_tail_tile_region(
        native_image, unbind.array_layer, 0, native_image.get_sparse_memory_requirements().mip_tail_size);
}

ID3D12Resource* get_dx12_sparse_resource(const SparseBufferBind& op)
{
    return static_cast<const Dx12BufferResource&>(*op.buffer).handle();
}

static ID3D12Resource* get_dx12_sparse_resource(const SparseBufferUnbind& op)
{
    return static_cast<const Dx12BufferResource&>(*op.buffer).handle();
}

template <typename ImageOp>
ID3D12Resource* get_dx12_sparse_resource(const ImageOp& op)
{
    return static_cast<const Dx12ImageResource&>(*op.image).handle();
}

void dx12_update_tile_mapping(
    ID3D12CommandQueue* queue,
    ID3D12Resource* resource,
    const Dx12TileRegion& region,
    const GpuHeap* heap,
    uint64_t heap_offset)
{
    const D3D12_TILE_RANGE_FLAGS range_flag = heap == nullptr ? D3D12_TILE_RANGE_FLAG_NULL : D3D12_TILE_RANGE_FLAG_NONE;
    ID3D12Heap* native_heap = heap == nullptr ? nullptr : static_cast<const Dx12GpuHeap*>(heap)->handle();

    const UINT heap_offset_in_tiles = heap == nullptr ? 0 : to_dx12_tile_index(heap_offset);
    const UINT range_tile_count = region.size.NumTiles;

    queue->UpdateTileMappings(
        resource,
        1,
        &region.coordinate,
        &region.size,
        native_heap,
        1,
        &range_flag,
        heap == nullptr ? nullptr : &heap_offset_in_tiles,
        &range_tile_count,
        D3D12_TILE_MAPPING_FLAG_NONE);
}

template <typename BindOp>
void dx12_apply_binds(ID3D12CommandQueue* queue, std::span<const BindOp> binds)
{
    for (const BindOp& bind : binds)
    {
        dx12_update_tile_mapping(
            queue, get_dx12_sparse_resource(bind), to_dx12_tile_region(bind), bind.heap, bind.heap_offset);
    }
}

template <typename UnbindOp>
void dx12_apply_unbinds(ID3D12CommandQueue* queue, std::span<const UnbindOp> unbinds)
{
    for (const UnbindOp& unbind : unbinds)
    {
        dx12_update_tile_mapping(queue, get_dx12_sparse_resource(unbind), to_dx12_tile_region(unbind), nullptr, 0);
    }
}

} // namespace

SyncPoint Dx12Queue::bind_sparse(const SparseBindBatch& batch, const SubmitInfo& info)
{
    MIZU_PROFILE_SCOPED;

    MIZU_ASSERT(
        batch.get_metadata_binds().empty() && batch.get_metadata_unbinds().empty(),
        "Sparse metadata is managed by the driver on Dx12, metadata_size is always 0");

    const std::lock_guard lock(m_mutex);

    for (const SyncPoint& wait : info.waits)
    {
        if (wait.is_valid())
            DX12_CHECK(m_queue->Wait(get_native_fence(wait), wait.value));
    }

    dx12_apply_unbinds(m_queue, batch.get_buffer_unbinds());
    dx12_apply_unbinds(m_queue, batch.get_image_unbinds());
    dx12_apply_unbinds(m_queue, batch.get_mip_tail_unbinds());

    dx12_apply_binds(m_queue, batch.get_buffer_binds());
    dx12_apply_binds(m_queue, batch.get_image_binds());
    dx12_apply_binds(m_queue, batch.get_mip_tail_binds());

    m_last_value += 1;
    DX12_CHECK(m_queue->Signal(m_fence->handle(), m_last_value));

    for (const SyncPoint& signal : info.signals)
    {
        DX12_CHECK(m_queue->Signal(get_native_fence(signal), signal.value));
    }

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
