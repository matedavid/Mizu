#include "resource_view.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"

namespace Mizu
{

ImageResourceViewRange ImageResourceViewRange::from_mips_layers(
    uint32_t mip_base,
    uint32_t mip_count,
    uint32_t layer_base,
    uint32_t layer_count)
{
    ImageResourceViewRange range{};
    range.m_mip_base = mip_base;
    range.m_mip_count = mip_count;
    range.m_layer_base = layer_base;
    range.m_layer_count = layer_count;

    return range;
}

ImageResourceViewRange ImageResourceViewRange::from_mips(uint32_t mip_base, uint32_t mip_count)
{
    ImageResourceViewRange range{};
    range.m_mip_base = mip_base;
    range.m_mip_count = mip_count;

    return range;
}

ImageResourceViewRange ImageResourceViewRange::from_layers(uint32_t layer_base, uint32_t layer_count)
{
    ImageResourceViewRange range{};
    range.m_layer_base = layer_base;
    range.m_layer_count = layer_count;

    return range;
}

std::shared_ptr<ImageResourceView> ImageResourceView::create(
    std::shared_ptr<ImageResource> resource,
    ImageResourceViewRange range)
{
    return create(resource, resource->get_format(), range);
}

std::shared_ptr<ImageResourceView> ImageResourceView::create(
    std::shared_ptr<ImageResource> resource,
    ImageFormat format,
    ImageResourceViewRange range)
{
#if MIZU_DEBUG
    const uint32_t max_mip = range.get_mip_base() + range.get_mip_count() - 1;
    MIZU_ASSERT(
        range.get_mip_base() < resource->get_num_mips() && max_mip < resource->get_num_mips(),
        "Mip range is not valid (num_mips = {}, range = ({},{}))",
        resource->get_num_mips(),
        range.get_mip_base(),
        max_mip);

    const uint32_t max_layer = range.get_layer_base() + range.get_layer_count() - 1;
    MIZU_ASSERT(
        range.get_layer_base() < resource->get_num_layers() && max_layer < resource->get_num_layers(),
        "Layer range is not valid (num_layers = {}, range = ({},{}))",
        resource->get_num_layers(),
        range.get_layer_base(),
        max_layer);
#endif

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12ImageResourceView>(resource, format, range);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResourceView>(resource, format, range);
    }
}

//
// NEW RESOURCE VIEWS FOR D3D12
//

//
// ShaderResourceView
//

std::shared_ptr<ShaderResourceView> ShaderResourceView::create(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12ShaderResourceView>(resource, range);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<ShaderResourceView> ShaderResourceView::create(const std::shared_ptr<BufferResource>& resource)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12ShaderResourceView>(resource);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

//
// UnorderedAccessView
//

std::shared_ptr<UnorderedAccessView> UnorderedAccessView::create(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12UnorderedAccessView>(resource, range);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<UnorderedAccessView> UnorderedAccessView::create(const std::shared_ptr<BufferResource>& resource)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12UnorderedAccessView>(resource);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

//
// ConstantBufferView
//

std::shared_ptr<ConstantBufferView> ConstantBufferView::create(const std::shared_ptr<BufferResource>& resource)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12ConstantBufferView>(resource);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

//
// RenderTargetView
//

std::shared_ptr<RenderTargetView> RenderTargetView::create(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)
{
    return create(resource, resource->get_format(), range);
}

std::shared_ptr<RenderTargetView> RenderTargetView::create(
    const std::shared_ptr<ImageResource>& resource,
    ImageFormat format,
    ImageResourceViewRange range)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12RenderTargetView>(resource, format, range);
    case GraphicsAPI::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
