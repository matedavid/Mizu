#include "dx12_image_resource.h"

#include <array>

#include "dx12_context.h"
#include "dx12_debug.h"
#include "dx12_resource_view.h"

namespace Mizu::Dx12
{

Dx12ImageResource::Dx12ImageResource(ImageDescription desc) : m_description(std::move(desc)), m_owns_resources(true)
{
    m_image_resource_description = D3D12_RESOURCE_DESC{};
    m_image_resource_description.Dimension = get_dx12_image_type(m_description.type);
    m_image_resource_description.Alignment = 0;
    m_image_resource_description.Width = m_description.width;
    m_image_resource_description.Height = m_description.height;
    m_image_resource_description.DepthOrArraySize = static_cast<uint16_t>(m_description.depth);
    m_image_resource_description.MipLevels = static_cast<uint16_t>(m_description.num_mips);
    m_image_resource_description.Format = get_dx12_image_format(m_description.format);
    m_image_resource_description.SampleDesc = DXGI_SAMPLE_DESC{.Count = 1, .Quality = 0};
    m_image_resource_description.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    m_image_resource_description.Flags = D3D12_RESOURCE_FLAG_NONE;

    if (m_description.usage & ImageUsageBits::Attachment && is_depth_format(m_description.format))
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    else if (m_description.usage & ImageUsageBits::Attachment)
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    if (m_description.usage & ImageUsageBits::UnorderedAccess)
        m_image_resource_description.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (!m_description.is_virtual)
    {
        m_allocation_info = Dx12Context.default_device_allocator->allocate_image_resource(*this);

        ID3D12Heap* heap = static_cast<ID3D12Heap*>(m_allocation_info.device_memory);
        create_placed_resource(heap, m_allocation_info.offset);
    }
}

Dx12ImageResource::Dx12ImageResource(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    ImageUsageBits usage,
    ID3D12Resource* image,
    bool owns_resources)
{
    m_description.width = width;
    m_description.height = height;
    m_description.format = format;
    m_description.usage = usage;

    m_owns_resources = owns_resources;

    m_resource = image;
}

Dx12ImageResource::~Dx12ImageResource()
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr)
            continue;

        const Dx12ImageResourceView* internal_view = reinterpret_cast<const Dx12ImageResourceView*>(view.internal);
        free_image_cpu_descriptor_handle(internal_view->handle, view.view_type, internal_view->format);
        delete internal_view;
    }

    if (m_owns_resources)
    {
        if (!m_description.is_virtual)
        {
            Dx12Context.default_device_allocator->release(m_allocation_info.id);
        }

        m_resource->Release();
    }
}

ResourceView Dx12ImageResource::as_srv(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Sampled,
        "Trying to create srv for image '{}' that was not created with Sampled usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

ResourceView Dx12ImageResource::as_uav(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::UnorderedAccess,
        "Trying to create uav for image '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

ResourceView Dx12ImageResource::as_rtv(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Attachment,
        "Trying to create rtv for image '{}' that was not created with Attachment usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::RenderTargetView, desc);
}

ResourceView Dx12ImageResource::get_or_create_resource_view(
    ResourceViewType type,
    const ImageResourceViewDescription& desc)
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr || view.view_type != type)
            continue;

        const Dx12ImageResourceView* internal_view = reinterpret_cast<Dx12ImageResourceView*>(view.internal);
        if (internal_view->description == desc)
            return view;
    }

    Dx12ImageResourceView* internal_view = new Dx12ImageResourceView{};
    internal_view->description = desc;
    internal_view->format = desc.override_format.value_or(m_description.format);
    internal_view->handle = create_image_cpu_descriptor_handle(desc, *this, type);

    ResourceView view{};
    view.view_type = type;
    view.internal = internal_view;

    m_resource_views.push_back(view);

    return view;
}

MemoryRequirements Dx12ImageResource::get_memory_requirements() const
{
    const D3D12_RESOURCE_ALLOCATION_INFO allocation_info =
        Dx12Context.device->handle()->GetResourceAllocationInfo(0, 1, &m_image_resource_description);

    MemoryRequirements reqs{};
    reqs.size = allocation_info.SizeInBytes;
    reqs.alignment = allocation_info.Alignment;

    return reqs;
}

ImageMemoryRequirements Dx12ImageResource::get_image_memory_requirements() const
{
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    uint64_t total_size = 0;

    get_copyable_footprints(&footprint, nullptr, nullptr, &total_size);

    ImageMemoryRequirements reqs{};
    reqs.size = total_size;
    reqs.offset = footprint.Offset;
    reqs.row_pitch = footprint.Footprint.RowPitch;

    return reqs;
}

void Dx12ImageResource::get_copyable_footprints(
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
    uint32_t* num_rows,
    uint64_t* row_size_in_bytes,
    uint64_t* total_size) const
{
    Dx12Context.device->handle()->GetCopyableFootprints(
        &m_image_resource_description, 0, 1, 0, footprints, num_rows, row_size_in_bytes, total_size);
}

DXGI_FORMAT Dx12ImageResource::get_dx12_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::R16G16_SFLOAT:
        return DXGI_FORMAT_R16G16_FLOAT;
    case ImageFormat::R32G32_SFLOAT:
        return DXGI_FORMAT_R32G32_FLOAT;
    case ImageFormat::R32G32B32_SFLOAT:
        return DXGI_FORMAT_R32G32B32_FLOAT;
    case ImageFormat::R8G8B8A8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case ImageFormat::R8G8B8A8_UNORM:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::R16G16B16A16_SFLOAT:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case ImageFormat::R32G32B32A32_SFLOAT:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case ImageFormat::B8G8R8A8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case ImageFormat::B8G8R8A8_UNORM:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
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
    case ImageType::Cubemap:
        MIZU_UNREACHABLE("Not implemented");
        return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    }

    MIZU_UNREACHABLE("Invalid ImageType");
}

D3D12_RESOURCE_STATES Dx12ImageResource::get_dx12_image_resource_state(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return D3D12_RESOURCE_STATE_COMMON;
    case ImageResourceState::UnorderedAccess:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    case ImageResourceState::TransferDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;
    case ImageResourceState::ShaderReadOnly:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    case ImageResourceState::ColorAttachment:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;
    case ImageResourceState::DepthStencilAttachment:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;
    case ImageResourceState::Present:
        return D3D12_RESOURCE_STATE_PRESENT;
    }
}

D3D12_BARRIER_LAYOUT Dx12ImageResource::get_dx12_image_barrier_layout(ImageResourceState state, ImageFormat format)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return D3D12_BARRIER_LAYOUT_COMMON;
    case ImageResourceState::UnorderedAccess:
        return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
    case ImageResourceState::TransferDst:
        return D3D12_BARRIER_LAYOUT_COPY_DEST;
    case ImageResourceState::ShaderReadOnly: {
        if (is_depth_format(format))
            return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
        else
            return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
    }
    case ImageResourceState::ColorAttachment:
        return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
    case ImageResourceState::DepthStencilAttachment:
        return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;
    case ImageResourceState::Present:
        return D3D12_BARRIER_LAYOUT_PRESENT;
    }
}

void Dx12ImageResource::create_placed_resource(ID3D12Heap* heap, uint64_t offset)
{
    DX12_CHECK(Dx12Context.device->handle()->CreatePlacedResource(
        heap, offset, &m_image_resource_description, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&m_resource)));

    if (!m_description.name.empty())
    {
        DX12_DEBUG_SET_RESOURCE_NAME(m_resource, m_description.name);
    }
}

} // namespace Mizu::Dx12