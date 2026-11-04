#include "dx12_buffer_resource.h"

#include <algorithm>

#include "base/math/math.h"

#include "dx12_context.h"
#include "dx12_debug.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

Dx12BufferResource::Dx12BufferResource(BufferDescription desc) : m_description(std::move(desc))
{
    /*
    m_buffer_resource_description = get_dx12_resource_desc(m_description);

    if (!m_description.is_virtual)
    {
        Dx12BaseDeviceMemoryAllocator& allocator =
            dynamic_cast<Dx12BaseDeviceMemoryAllocator&>(*Dx12Context.default_device_allocator.get());

        m_allocation_info = allocator.allocate_buffer_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);

        if (m_description.usage & BufferUsageBits::HostVisible)
        {
            map();
        }
    }
    */
}

Dx12BufferResource::Dx12BufferResource(const BufferDescription& desc, const MemoryPlacement& placement)
    : m_sparse(false)
    , m_description(desc)
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);

    const Dx12GpuHeap& native_heap = static_cast<const Dx12GpuHeap&>(*placement.heap);
    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource2(
        native_heap.handle(),
        placement.offset,
        &resource_desc,
        D3D12_BARRIER_LAYOUT_UNDEFINED,
        nullptr,
        0,
        nullptr,
        IID_PPV_ARGS(&m_resource)));

    DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);

    init_memory_requirements();

    if (native_heap.is_host_visible())
    {
        DX12_CHECK(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&m_mapped_memory)));
    }
}

Dx12BufferResource::Dx12BufferResource(const SparseBufferDescription& desc) : m_sparse(true), m_description(desc.desc)
{
    const D3D12_RESOURCE_DESC1 resource_desc1 = get_dx12_resource_desc(m_description);

    // TODO: Not the happiest person right now
    D3D12_RESOURCE_DESC resource_desc{};
    resource_desc.Dimension = resource_desc1.Dimension;
    resource_desc.Alignment = resource_desc1.Alignment;
    resource_desc.Width = resource_desc1.Width;
    resource_desc.Height = resource_desc1.Height;
    resource_desc.DepthOrArraySize = resource_desc1.DepthOrArraySize;
    resource_desc.MipLevels = resource_desc1.MipLevels;
    resource_desc.Format = resource_desc1.Format;
    resource_desc.SampleDesc = resource_desc1.SampleDesc;
    resource_desc.Layout = resource_desc1.Layout;
    // Tight alignment is not valid for reserved resources, they are always 64 KiB tiled
    resource_desc.Flags = resource_desc1.Flags & ~D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

    DX12_CHECK(Dx12Context.device->handle()->CreateReservedResource2(
        &resource_desc, D3D12_BARRIER_LAYOUT_UNDEFINED, nullptr, nullptr, 0, nullptr, IID_PPV_ARGS(&m_resource)));

    DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);

    init_memory_requirements();
}

Dx12BufferResource::~Dx12BufferResource()
{
    m_resource->Release();
}

uint8_t* Dx12BufferResource::get_mapped_memory() const
{
    return m_mapped_memory;
}

MemoryRequirements Dx12BufferResource::get_memory_requirements() const
{
    return m_memory_requirements;
}

void Dx12BufferResource::init_memory_requirements()
{
    m_memory_requirements = get_dx12_buffer_memory_requirements(m_description);

    if (m_sparse)
    {
        constexpr uint64_t TILE_SIZE = D3D12_TILED_RESOURCE_TILE_SIZE_IN_BYTES;

        m_memory_requirements.alignment = TILE_SIZE;
        m_memory_requirements.size = math::align_up(m_memory_requirements.size, TILE_SIZE);
    }
}

void Dx12BufferResource::get_copyable_footprints(
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
    uint32_t* num_rows,
    uint64_t* row_size_in_bytes,
    uint64_t* total_size) const
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(m_description);
    Dx12Context.device->handle()->GetCopyableFootprints1(
        &resource_desc, 0, 1, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

D3D12_RESOURCE_DESC1 get_dx12_resource_desc(const BufferDescription& desc)
{
    uint64_t size = desc.size;
    if (desc.usage & BufferUsageBits::ConstantBuffer)
    {
        // D3D12 ConstantBufferViews must be 256 bit aligned, modifying size in here to account for this
        size = (desc.size + 255) & ~255;
    }

    D3D12_RESOURCE_DESC1 resource_desc{};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = get_dx12_buffer_usage(desc.usage);

    // Use tight alignment. Acceleration structure buffers are the exception: D3D12 rejects
    // D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT together with D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE.
    if (!(resource_desc.Flags & D3D12_RESOURCE_FLAG_RAYTRACING_ACCELERATION_STRUCTURE))
        resource_desc.Flags |= D3D12_RESOURCE_FLAG_USE_TIGHT_ALIGNMENT;

    // D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS is not required for buffers, only for textures.
    // See: https://learn.microsoft.com/en-us/windows-hardware/drivers/display/enhanced-barriers

    return resource_desc;
}

MemoryRequirements get_dx12_buffer_memory_requirements(const BufferDescription& desc)
{
    const D3D12_RESOURCE_DESC1 resource_desc = get_dx12_resource_desc(desc);

    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo2(0, 1, &resource_desc, nullptr);

    const bool heap_tier2 =
        Dx12Context.device->get_feature_data_options().ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2;

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;
    reqs.memory_type_bits = heap_tier2 ? MEMORY_TYPE_BITS_ALL : MEMORY_TYPE_BITS_BUFFER;

    // Tight alignment can place buffers at addresses as low as 8 byte aligned, but ray tracing structures have
    // stricter GPU virtual address requirements (relevant when sub-allocating from a shared heap).
    if (desc.usage & BufferUsageBits::RtxShaderBindingTable)
        reqs.alignment = std::max<uint64_t>(reqs.alignment, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    if (desc.usage
        & (BufferUsageBits::RtxAccelerationStructureStorage | BufferUsageBits::RtxAccelerationStructureInputReadOnly))
        reqs.alignment = std::max<uint64_t>(reqs.alignment, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);

    return reqs;
}

} // namespace Mizu::Dx12
