#include "dx12_resource_view.h"

#include "dx12_buffer_resource.h"
#include "dx12_context.h"
#include "dx12_image_resource.h"

namespace Mizu::Dx12
{

static D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_srv_cpu_descriptor_handle(const Dx12BufferResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();

    const bool is_structured_buffer = resource.get_stride() != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t num_elements = is_structured_buffer
                                      ? static_cast<uint32_t>(resource.get_size() / resource.get_stride())
                                      : static_cast<uint32_t>(resource.get_size() / 4);
    const D3D12_BUFFER_SRV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_SRV_FLAG_NONE : D3D12_BUFFER_SRV_FLAG_RAW;

    D3D12_BUFFER_SRV buffer_srv{};
    buffer_srv.FirstElement = 0;
    buffer_srv.NumElements = num_elements;
    buffer_srv.StructureByteStride = static_cast<uint32_t>(resource.get_stride());
    buffer_srv.Flags = buffer_flags;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Buffer = buffer_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(resource.handle(), &srv_desc, handle);

    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_uav_cpu_descriptor_handle(const Dx12BufferResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();

    const bool is_structured_buffer = resource.get_stride() != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t num_elements = is_structured_buffer
                                      ? static_cast<uint32_t>(resource.get_size() / resource.get_stride())
                                      : static_cast<uint32_t>(resource.get_size() / 4);
    const D3D12_BUFFER_UAV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_UAV_FLAG_NONE : D3D12_BUFFER_UAV_FLAG_RAW;

    D3D12_BUFFER_UAV buffer_uav{};
    buffer_uav.FirstElement = 0;
    buffer_uav.NumElements = num_elements;
    buffer_uav.StructureByteStride = static_cast<uint32_t>(resource.get_stride());
    buffer_uav.CounterOffsetInBytes = 0;
    buffer_uav.Flags = buffer_flags;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer = buffer_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(resource.handle(), nullptr, &uav_desc, handle);

    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_cbv_cpu_descriptor_handle(const Dx12BufferResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();

    const uint64_t aligned_size = (resource.get_size() + 255) & ~255; // CB size must be 256-byte aligned.

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
    cbv_desc.BufferLocation = resource.get_gpu_address();
    cbv_desc.SizeInBytes = static_cast<uint32_t>(aligned_size);

    Dx12Context.device->handle()->CreateConstantBufferView(&cbv_desc, handle);

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE create_buffer_cpu_descriptor_handle(
    const Dx12BufferResource& resource,
    ResourceViewType type)
{
    switch (type)
    {
    case ResourceViewType::ShaderResourceView:
        return create_buffer_srv_cpu_descriptor_handle(resource);
    case ResourceViewType::UnorderedAccessView:
        return create_buffer_uav_cpu_descriptor_handle(resource);
    case ResourceViewType::ConstantBufferView:
        return create_buffer_cbv_cpu_descriptor_handle(resource);
    default:
        MIZU_UNREACHABLE("Unsupported ResourceViewType for buffer resource view");
        break;
    }

    return {};
}

void free_buffer_cpu_descriptor_handle(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    Dx12Context.heaps.cbv_srv_uav_heap->free(handle);
}

static D3D12_CPU_DESCRIPTOR_HANDLE create_image_srv_cpu_descriptor_handle(
    const ImageResourceViewDescription& desc,
    const Dx12ImageResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    D3D12_TEX2D_SRV texture_srv{};
    texture_srv.MostDetailedMip = desc.layer_base;
    texture_srv.MipLevels = desc.mip_count;
    texture_srv.PlaneSlice = desc.layer_base;
    texture_srv.ResourceMinLODClamp = 0.0f;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = Dx12ImageResource::get_dx12_image_format(format);
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D = texture_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(resource.handle(), &srv_desc, handle);

    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE create_image_uav_cpu_descriptor_handle(
    const ImageResourceViewDescription& desc,
    const Dx12ImageResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = Dx12Context.heaps.cbv_srv_uav_heap->allocate();
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    D3D12_TEX2D_UAV texture_uav{};
    texture_uav.MipSlice = desc.mip_base;
    texture_uav.PlaneSlice = desc.layer_base;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = Dx12ImageResource::get_dx12_image_format(format);
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D = texture_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(resource.handle(), nullptr, &uav_desc, handle);

    return handle;
}

static D3D12_CPU_DESCRIPTOR_HANDLE create_image_rtv_cpu_descriptor_handle(
    const ImageResourceViewDescription& desc,
    const Dx12ImageResource& resource)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    const bool is_depth = is_depth_format(format);

    if (is_depth)
    {
        handle = Dx12Context.heaps.dsv_heap->allocate();
    }
    else
    {
        handle = Dx12Context.heaps.rtv_heap->allocate();
    }

    if (!is_depth)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
        rtv_desc.Format = Dx12ImageResource::get_dx12_image_format(format);
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = desc.mip_base;
        rtv_desc.Texture2D.PlaneSlice = desc.layer_base;

        Dx12Context.device->handle()->CreateRenderTargetView(resource.handle(), &rtv_desc, handle);
    }
    else
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format = Dx12ImageResource::get_dx12_image_format(format);
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Texture2D.MipSlice = desc.mip_base;

        Dx12Context.device->handle()->CreateDepthStencilView(resource.handle(), &dsv_desc, handle);
    }

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE create_image_cpu_descriptor_handle(
    const ImageResourceViewDescription& desc,
    const Dx12ImageResource& resource,
    ResourceViewType type)
{
    switch (type)
    {
    case ResourceViewType::ShaderResourceView:
        return create_image_srv_cpu_descriptor_handle(desc, resource);
    case ResourceViewType::UnorderedAccessView:
        return create_image_uav_cpu_descriptor_handle(desc, resource);
    case ResourceViewType::RenderTargetView:
        return create_image_rtv_cpu_descriptor_handle(desc, resource);
    default:
        MIZU_UNREACHABLE("Unsupported ResourceViewType for image resource view");
        break;
    }

    return {};
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