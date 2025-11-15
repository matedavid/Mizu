#include "dx12_buffer_resource.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_device_memory_allocator.h"

namespace Mizu::Dx12
{

Dx12BufferResource::Dx12BufferResource(BufferDescription desc) : m_description(std::move(desc))
{
    uint64_t size = m_description.size;
    if (m_description.usage & BufferUsageBits::ConstantBuffer)
    {
        // D3D12 ConstantBufferViews must be 256 bit aligned, modifying size in here to account for this
        size = (m_description.size + 255) & ~255;
    }

    m_buffer_resource_description = D3D12_RESOURCE_DESC{};
    m_buffer_resource_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    m_buffer_resource_description.Alignment = 0;
    m_buffer_resource_description.Width = size;
    m_buffer_resource_description.Height = 1;
    m_buffer_resource_description.DepthOrArraySize = 1;
    m_buffer_resource_description.MipLevels = 1;
    m_buffer_resource_description.Format = DXGI_FORMAT_UNKNOWN;
    m_buffer_resource_description.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    m_buffer_resource_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    m_buffer_resource_description.Flags = get_dx12_usage(m_description.usage);

    if (!m_description.is_virtual)
    {
        Dx12BaseDeviceMemoryAllocator& allocator =
            dynamic_cast<Dx12BaseDeviceMemoryAllocator&>(*Renderer::get_allocator().get());

        m_allocation_info = allocator.allocate_buffer_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);

        allocator.map_memory_if_host_visible(*this, m_allocation_info.id);
    }
}

Dx12BufferResource::~Dx12BufferResource()
{
    if (!m_description.is_virtual)
    {
        Renderer::get_allocator()->release(m_allocation_info.id);
    }

    m_resource->Release();
}

MemoryRequirements Dx12BufferResource::get_memory_requirements() const
{
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(0, 1, &m_buffer_resource_description);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

void Dx12BufferResource::set_data(const uint8_t* data, size_t size, size_t offset) const
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::HostVisible, "Can't map data that does not have the HostVisible usage");

    uint8_t* mapped = Renderer::get_allocator()->get_mapped_memory(m_allocation_info.id);
    MIZU_ASSERT(mapped != nullptr, "Memory is not mapped");

    memcpy(mapped + m_allocation_info.offset + offset, data, size);
}

void Dx12BufferResource::get_copyable_footprints(
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
    uint32_t* num_rows,
    uint64_t* row_size_in_bytes,
    uint64_t* total_size) const
{
    Dx12Context.device->handle()->GetCopyableFootprints(
        &m_buffer_resource_description, 0, 1, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

D3D12_RESOURCE_FLAGS Dx12BufferResource::get_dx12_usage(BufferUsageBits usage)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (usage & BufferUsageBits::UnorderedAccess)
        flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    return flags;
}

D3D12_RESOURCE_STATES Dx12BufferResource::get_dx12_buffer_resource_state(BufferResourceState state)
{
    switch (state)
    {
    case BufferResourceState::Undefined:
        return D3D12_RESOURCE_STATE_COMMON;
    case BufferResourceState::General:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case BufferResourceState::TransferSrc:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;
    case BufferResourceState::TransferDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case BufferResourceState::ShaderReadOnly:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    }
}

void Dx12BufferResource::create_placed_resource(ID3D12Heap* heap, uint64_t offset)
{
    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource(
        heap, offset, &m_buffer_resource_description, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));
}

} // namespace Mizu::Dx12