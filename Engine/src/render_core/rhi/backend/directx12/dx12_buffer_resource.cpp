#include "dx12_buffer_resource.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

Dx12BufferResource::Dx12BufferResource(BufferDescription desc) : m_description(std::move(desc))
{
    m_buffer_resource_description = D3D12_RESOURCE_DESC{};
    m_buffer_resource_description.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    m_buffer_resource_description.Alignment = 0;
    m_buffer_resource_description.Width = m_description.size;
    m_buffer_resource_description.Height = 1;
    m_buffer_resource_description.DepthOrArraySize = 1;
    m_buffer_resource_description.MipLevels = 1;
    m_buffer_resource_description.Format = DXGI_FORMAT_UNKNOWN;
    m_buffer_resource_description.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    m_buffer_resource_description.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    m_buffer_resource_description.Flags = get_dx12_usage(m_description.usage);

    // TODO: For the moment using CreateCommittedResource, should change to CreatePlacedResource so that it aligns with
    // the memory model from the Vulkan implementation.

    D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;

    if (m_description.usage & BufferUsageBits::HostVisible)
        heap_type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = heap_type;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 1; // TODO: Not sure what this does, investigate
    heap_properties.VisibleNodeMask = 1;  // TODO: Not sure what this does, investigate

    DX12_CHECK(Dx12Context.device->handle()->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &m_buffer_resource_description,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_resource)));

    if (m_description.usage & BufferUsageBits::HostVisible)
    {
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12resource-map
        // pReadRange
        // A null pointer indicates the entire subresource might be read by the CPU.
        DX12_CHECK(m_resource->Map(0, nullptr, reinterpret_cast<void**>(&m_mapped_data)));
    }
}

Dx12BufferResource::~Dx12BufferResource()
{
    if (m_mapped_data != nullptr)
        m_resource->Unmap(0, nullptr);

    m_resource->Release();
}

MemoryRequirements Dx12BufferResource::get_memory_requirements() const
{
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(1, 1, &m_buffer_resource_description);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

void Dx12BufferResource::set_data(const uint8_t* data) const
{
    MIZU_ASSERT(m_mapped_data != nullptr, "Memory is not mapped");

    memcpy(m_mapped_data, data, m_description.size);
}

D3D12_RESOURCE_FLAGS Dx12BufferResource::get_dx12_usage(BufferUsageBits usage)
{
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;

    if (usage & BufferUsageBits::StorageBuffer)
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
} // namespace Mizu::Dx12

} // namespace Mizu::Dx12