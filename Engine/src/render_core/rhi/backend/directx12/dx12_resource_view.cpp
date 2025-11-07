#include "dx12_resource_view.h"

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
    (void)resource;
    (void)range;
}

Dx12ShaderResourceView::Dx12ShaderResourceView(std::shared_ptr<BufferResource> resource)
{
    (void)resource;
}

//
// Dx12UnorderedAccessView
//

Dx12UnorderedAccessView::Dx12UnorderedAccessView(std::shared_ptr<ImageResource> resource, ImageResourceViewRange range)
{
    (void)resource;
    (void)range;
}

Dx12UnorderedAccessView::Dx12UnorderedAccessView(std::shared_ptr<BufferResource> resource)
{
    (void)resource;
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
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = 1;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    m_handle = D3D12_CPU_DESCRIPTOR_HANDLE(m_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

    const Dx12ImageResource& native_resource = dynamic_cast<const Dx12ImageResource&>(*resource);

    D3D12_RENDER_TARGET_VIEW_DESC rtv_desc{};
    rtv_desc.Format = Dx12ImageResource::get_dx12_image_format(m_format);
    rtv_desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = m_range.get_mip_base();
    rtv_desc.Texture2D.PlaneSlice = m_range.get_layer_base();

    Dx12Context.device->handle()->CreateRenderTargetView(native_resource.handle(), &rtv_desc, m_handle);
}

Dx12RenderTargetView::~Dx12RenderTargetView()
{
    m_descriptor_heap->Release();
}

} // namespace Mizu::Dx12