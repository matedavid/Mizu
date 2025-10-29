#include "dx12_image_resource.h"

#include <array>

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

Dx12ImageResource::Dx12ImageResource(ImageDescription desc) : m_description(std::move(desc))
{
    m_image_resource_description = D3D12_RESOURCE_DESC{};
    m_image_resource_description.Dimension = get_dx12_image_type(m_description.type);
    m_image_resource_description.Alignment = 0;
    m_image_resource_description.Width = m_description.width;
    m_image_resource_description.Height = m_description.height;
    m_image_resource_description.DepthOrArraySize = static_cast<uint16_t>(m_description.depth);
    m_image_resource_description.MipLevels = static_cast<uint16_t>(m_description.num_mips);
    m_image_resource_description.Format = get_image_format(m_description.format);
    m_image_resource_description.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    m_image_resource_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    m_image_resource_description.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (m_description.usage & ImageUsageBits::Attachment && ImageUtils::is_depth_format(m_description.format))
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    else if (m_description.usage & ImageUsageBits::Attachment)
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    if (m_description.usage & ImageUsageBits::Storage)
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    // TODO: For the moment using CreateCommittedResource, should change to CreatePlacedResource so that it aligns with
    // the memory model from the Vulkan implementation.

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 1; // TODO: Not sure what this does, investigate
    heap_properties.VisibleNodeMask = 1;  // TODO: Not sure what this does, investigate

    DX12_CHECK(Dx12Context.device->handle()->CreateCommittedResource(
        &heap_properties,
        D3D12_HEAP_FLAG_NONE,
        &m_image_resource_description,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_resource)));
}

Dx12ImageResource::~Dx12ImageResource()
{
    m_resource->Release();
}

MemoryRequirements Dx12ImageResource::get_memory_requirements() const
{
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(1, 1, &m_image_resource_description);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

DXGI_FORMAT Dx12ImageResource::get_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::RG16_SFLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case ImageFormat::RG32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::RGB32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImageFormat::RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImageFormat::RGBA32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImageFormat::BGRA8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case ImageFormat::D32_SFLOAT:
        return DXGI_FORMAT_D32_FLOAT;
    }
}

D3D12_RESOURCE_DIMENSION Dx12ImageResource::get_dx12_image_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
    case ImageType::Image2D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    case ImageType::Image3D:
        return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    }

    MIZU_UNREACHABLE("Invalid ImageType");

    return D3D12_RESOURCE_DIMENSION_TEXTURE2D; // Return default to prevent compilation errors
}

} // namespace Mizu::Dx12