#include "dx12_resource_view.h"

#include "base/debug/logging.h"

#include "dx12_buffer_resource.h"
#include "dx12_context.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

void create_buffer_srv(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const uint32_t stride = static_cast<uint32_t>(resource.get_stride());
    const bool is_structured_buffer = stride != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t element_size = is_structured_buffer ? stride : 4u;
    const D3D12_BUFFER_SRV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_SRV_FLAG_NONE : D3D12_BUFFER_SRV_FLAG_RAW;

    const uint32_t first_element = static_cast<uint32_t>(desc.offset / element_size);
    const uint32_t num_elements = static_cast<uint32_t>((desc.size + element_size - 1) / element_size);

    D3D12_BUFFER_SRV buffer_srv{};
    buffer_srv.FirstElement = first_element;
    buffer_srv.NumElements = num_elements;
    buffer_srv.StructureByteStride = stride;
    buffer_srv.Flags = buffer_flags;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Buffer = buffer_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(resource.handle(), &srv_desc, handle);
}

void create_buffer_uav(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const uint32_t stride = static_cast<uint32_t>(resource.get_stride());
    const bool is_structured_buffer = stride != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t element_size = is_structured_buffer ? stride : 4u;
    const D3D12_BUFFER_UAV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_UAV_FLAG_NONE : D3D12_BUFFER_UAV_FLAG_RAW;

    const uint32_t first_element = static_cast<uint32_t>(desc.offset / element_size);
    const uint32_t num_elements = static_cast<uint32_t>((desc.size + element_size - 1) / element_size);

    D3D12_BUFFER_UAV buffer_uav{};
    buffer_uav.FirstElement = first_element;
    buffer_uav.NumElements = num_elements;
    buffer_uav.StructureByteStride = stride;
    buffer_uav.CounterOffsetInBytes = 0;
    buffer_uav.Flags = buffer_flags;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer = buffer_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(resource.handle(), nullptr, &uav_desc, handle);
}

void create_buffer_cbv(
    const Dx12BufferResource& resource,
    const BufferResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    MIZU_ASSERT(
        (desc.offset % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0,
        "CBV offset {} is not aligned to {}",
        desc.offset,
        D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

    const uint64_t aligned_size = (desc.size + D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1)
                                  & ~(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
    cbv_desc.BufferLocation = resource.get_gpu_address() + desc.offset;
    cbv_desc.SizeInBytes = static_cast<uint32_t>(aligned_size);

    Dx12Context.device->handle()->CreateConstantBufferView(&cbv_desc, handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_cpu_descriptor_handle(
    const Dx12BufferResource& resource,
    ResourceViewType type,
    const BufferResourceViewDescription& desc)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();

    switch (type)
    {
    case ResourceViewType::ShaderResourceView:
        create_buffer_srv(resource, desc, handle);
        break;
    case ResourceViewType::UnorderedAccessView:
        create_buffer_uav(resource, desc, handle);
        break;
    case ResourceViewType::ConstantBufferView:
        create_buffer_cbv(resource, desc, handle);
        break;
    default:
        MIZU_UNREACHABLE("Unsupported ResourceViewType for buffer resource view");
        break;
    }

    return handle;
}

void free_buffer_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    Dx12Context.heaps.cbv_srv_uav_heap->free(handle);
}

void create_image_srv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    D3D12_TEX2D_SRV texture_srv{};
    texture_srv.MostDetailedMip = desc.layer_base;
    texture_srv.MipLevels = desc.mip_count;
    texture_srv.PlaneSlice = desc.layer_base;
    texture_srv.ResourceMinLODClamp = 0.0f;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = get_dx12_image_format(format);
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D = texture_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(resource.handle(), &srv_desc, handle);
}

void create_image_uav(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    D3D12_TEX2D_UAV texture_uav{};
    texture_uav.MipSlice = desc.mip_base;
    texture_uav.PlaneSlice = desc.layer_base;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = get_dx12_image_format(format);
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D = texture_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(resource.handle(), nullptr, &uav_desc, handle);
}

void create_image_rtv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());
    MIZU_ASSERT(!is_depth_format(format), "Image must not have depth format");

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
    rtv_desc.Format = get_dx12_image_format(format);
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = desc.mip_base;
    rtv_desc.Texture2D.PlaneSlice = desc.layer_base;

    Dx12Context.device->handle()->CreateRenderTargetView(resource.handle(), &rtv_desc, handle);
}

void create_image_dsv(
    const Dx12ImageResource& resource,
    const ImageResourceViewDescription& desc,
    D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());
    MIZU_ASSERT(is_depth_format(format), "Image must have depth format");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
    dsv_desc.Format = get_dx12_image_format(format);
    dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv_desc.Texture2D.MipSlice = desc.mip_base;

    Dx12Context.device->handle()->CreateDepthStencilView(resource.handle(), &dsv_desc, handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE create_image_cpu_descriptor_handle(
    const Dx12ImageResource& resource,
    ResourceViewType type,
    const ImageResourceViewDescription& desc)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    if (type == ResourceViewType::RenderTargetView && is_depth_format(format))
    {
        handle = Dx12Context.heaps.dsv_heap->allocate();
    }
    else if (type == ResourceViewType::RenderTargetView)
    {
        handle = Dx12Context.heaps.rtv_heap->allocate();
    }
    else
    {
        handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();
    }

    switch (type)
    {
    case ResourceViewType::ShaderResourceView:
        create_image_srv(resource, desc, handle);
        break;
    case ResourceViewType::UnorderedAccessView:
        create_image_uav(resource, desc, handle);
        break;
    case ResourceViewType::RenderTargetView:
        if (!is_depth_format(format))
            create_image_rtv(resource, desc, handle);
        else
            create_image_dsv(resource, desc, handle);
        break;
    default:
        MIZU_UNREACHABLE("Unsupported ResourceViewType for image resource view");
        break;
    }

    return handle;
}

void free_image_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle, ResourceViewType type, ImageFormat format)
{
    switch (type)
    {
    case ResourceViewType::ShaderResourceView:
    case ResourceViewType::UnorderedAccessView:
        Dx12Context.heaps.cbv_srv_uav_heap->free(handle);
        break;
    case ResourceViewType::RenderTargetView: {
        if (is_depth_format(format))
            Dx12Context.heaps.dsv_heap->free(handle);
        else
            Dx12Context.heaps.rtv_heap->free(handle);
        break;
    }
    default:
        MIZU_UNREACHABLE("Unsupported ResourceViewType for image resource view");
        break;
    }
}

Dx12BufferResourceView* get_internal_buffer_resource_view(const ResourceView& view)
{
    Dx12BufferResourceView* internal = reinterpret_cast<Dx12BufferResourceView*>(view.internal);
    MIZU_ASSERT(internal != nullptr, "Failed to get internal buffer resource view");

    return internal;
}

Dx12ImageResourceView* get_internal_image_resource_view(const ResourceView& view)
{
    Dx12ImageResourceView* internal = reinterpret_cast<Dx12ImageResourceView*>(view.internal);
    MIZU_ASSERT(internal != nullptr, "Failed to get internal image resource view");

    return internal;
}

} // namespace Mizu::Dx12