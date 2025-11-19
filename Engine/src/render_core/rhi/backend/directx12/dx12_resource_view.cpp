#include "dx12_resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_buffer_resource.h"
#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"

namespace Mizu::Dx12
{

Dx12ImageResourceView::Dx12ImageResourceView(
    std::shared_ptr<ImageResource> resource,
    ImageFormat format,
    ImageResourceViewRange range)
    : m_range(range)
    , m_format(format)
{
    (void)resource;
}

ImageFormat Dx12ImageResourceView::get_format() const
{
    return m_format;
}

ImageResourceViewRange Dx12ImageResourceView::get_range() const
{
    return m_range;
}

//
// NEW RESOURCE VIEWS FOR D3D12
//

//
// Dx12ShaderResourceView
//

Dx12ShaderResourceView::Dx12ShaderResourceView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range)
{
    MIZU_ASSERT(
        resource->get_usage() & ImageUsageBits::Sampled, "Can't create SRV of image without the Sampled usage bit");

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12ImageResource& native_resource = static_cast<const Dx12ImageResource&>(*resource);

    D3D12_TEX2D_SRV texture_srv{};
    texture_srv.MostDetailedMip = range.get_mip_base();
    texture_srv.MipLevels = range.get_mip_count();
    texture_srv.PlaneSlice = range.get_layer_base();
    texture_srv.ResourceMinLODClamp = 0.0f;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = Dx12ImageResource::get_dx12_image_format(native_resource.get_format());
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D = texture_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(native_resource.handle(), &srv_desc, m_handle);
}

Dx12ShaderResourceView::Dx12ShaderResourceView(std::shared_ptr<BufferResource> resource)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12BufferResource& native_resource = static_cast<const Dx12BufferResource&>(*resource);

    const bool is_structured_buffer = native_resource.get_stride() != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t num_elements = is_structured_buffer
                                      ? static_cast<uint32_t>(native_resource.get_size() / native_resource.get_stride())
                                      : static_cast<uint32_t>(native_resource.get_size() / 4);
    const D3D12_BUFFER_SRV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_SRV_FLAG_NONE : D3D12_BUFFER_SRV_FLAG_RAW;

    D3D12_BUFFER_SRV buffer_srv{};
    buffer_srv.FirstElement = 0;
    buffer_srv.NumElements = num_elements;
    buffer_srv.StructureByteStride = static_cast<uint32_t>(native_resource.get_stride());
    buffer_srv.Flags = buffer_flags;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Buffer = buffer_srv;

    Dx12Context.device->handle()->CreateShaderResourceView(native_resource.handle(), &srv_desc, m_handle);
}

Dx12ShaderResourceView::~Dx12ShaderResourceView()
{
    m_descriptor_heap->Release();
}

//
// Dx12UnorderedAccessView
//

Dx12UnorderedAccessView::Dx12UnorderedAccessView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range)
{
    MIZU_ASSERT(
        resource->get_usage() & ImageUsageBits::UnorderedAccess,
        "Can't create UAV of image without the UnorderedAccess usage bit");

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12ImageResource& native_resource = static_cast<const Dx12ImageResource&>(*resource);

    D3D12_TEX2D_UAV texture_uav{};
    texture_uav.MipSlice = range.get_mip_base();
    texture_uav.PlaneSlice = range.get_layer_base();

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = DXGI_FORMAT_UNKNOWN;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D = texture_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(native_resource.handle(), nullptr, &uav_desc, m_handle);
}

Dx12UnorderedAccessView::Dx12UnorderedAccessView(std::shared_ptr<BufferResource> resource)
{
    MIZU_ASSERT(
        resource->get_usage() & BufferUsageBits::UnorderedAccess,
        "Can't create UAV of buffer without UnorderedAccess usage bit");

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12BufferResource& native_resource = static_cast<const Dx12BufferResource&>(*resource);

    const bool is_structured_buffer = native_resource.get_stride() != 0;

    const DXGI_FORMAT format = is_structured_buffer ? DXGI_FORMAT_UNKNOWN : DXGI_FORMAT_R32_TYPELESS;
    const uint32_t num_elements = is_structured_buffer
                                      ? static_cast<uint32_t>(native_resource.get_size() / native_resource.get_stride())
                                      : static_cast<uint32_t>(native_resource.get_size() / 4);
    const D3D12_BUFFER_UAV_FLAGS buffer_flags =
        is_structured_buffer ? D3D12_BUFFER_UAV_FLAG_NONE : D3D12_BUFFER_UAV_FLAG_RAW;

    D3D12_BUFFER_UAV buffer_uav{};
    buffer_uav.FirstElement = 0;
    buffer_uav.NumElements = num_elements;
    buffer_uav.StructureByteStride = static_cast<uint32_t>(native_resource.get_stride());
    buffer_uav.CounterOffsetInBytes = 0;
    buffer_uav.Flags = buffer_flags;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = format;
    uav_desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer = buffer_uav;

    Dx12Context.device->handle()->CreateUnorderedAccessView(native_resource.handle(), nullptr, &uav_desc, m_handle);
}

Dx12UnorderedAccessView::~Dx12UnorderedAccessView()
{
    m_descriptor_heap->Release();
}

//
// Dx12ConstantBufferView
//

Dx12ConstantBufferView::Dx12ConstantBufferView(std::shared_ptr<BufferResource> resource)
{
    MIZU_ASSERT(
        resource->get_usage() & BufferUsageBits::ConstantBuffer,
        "Can't create CBV of buffer without ConstantBuffer usage bit");

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12BufferResource& native_resource = static_cast<const Dx12BufferResource&>(*resource);

    const uint64_t aligned_size = (native_resource.get_size() + 255) & ~255; // CB size must be 256-byte aligned.

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc{};
    cbv_desc.BufferLocation = native_resource.get_gpu_address();
    cbv_desc.SizeInBytes = static_cast<uint32_t>(aligned_size);

    Dx12Context.device->handle()->CreateConstantBufferView(&cbv_desc, m_handle);
}

Dx12ConstantBufferView::~Dx12ConstantBufferView()
{
    m_descriptor_heap->Release();
}

//
// Dx12RenderTargetView
//

Dx12RenderTargetView::Dx12RenderTargetView(
    std::shared_ptr<ImageResource> resource,
    ImageFormat format,
    ImageResourceViewRange range)
    : m_format(format)
    , m_range(range)
{
    MIZU_ASSERT(
        resource->get_usage() & ImageUsageBits::Attachment, "Can't create RTV with image without Attachment usage bit");

    const bool is_depth_format = ImageUtils::is_depth_format(format);

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = is_depth_format ? D3D12_DESCRIPTOR_HEAP_TYPE_DSV : D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12ImageResource& native_resource = static_cast<const Dx12ImageResource&>(*resource);

    if (!is_depth_format)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
        rtv_desc.Format = Dx12ImageResource::get_dx12_image_format(m_format);
        rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtv_desc.Texture2D.MipSlice = m_range.get_mip_base();
        rtv_desc.Texture2D.PlaneSlice = m_range.get_layer_base();

        Dx12Context.device->handle()->CreateRenderTargetView(native_resource.handle(), &rtv_desc, m_handle);
    }
    else
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format = Dx12ImageResource::get_dx12_image_format(m_format);
        dsv_desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv_desc.Texture2D.MipSlice = m_range.get_mip_base();

        Dx12Context.device->handle()->CreateDepthStencilView(native_resource.handle(), &dsv_desc, m_handle);
    }
}

Dx12RenderTargetView::~Dx12RenderTargetView()
{
    m_descriptor_heap->Release();
}

} // namespace Mizu::Dx12