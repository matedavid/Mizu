#include "render_core/rhi/resource_view.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

namespace Mizu
{

//
// ImageResourceViewRange
//

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

//
// ShaderResourceView
//

std::shared_ptr<ShaderResourceView> ShaderResourceView::create(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)
{
    (void)resource;
    (void)range;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<ShaderResourceView> ShaderResourceView::create(const std::shared_ptr<BufferResource>& resource)
{
    (void)resource;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
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
    (void)resource;
    (void)range;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

std::shared_ptr<UnorderedAccessView> UnorderedAccessView::create(const std::shared_ptr<BufferResource>& resource)
{
    (void)resource;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

//
// ConstantBufferView
//

std::shared_ptr<ConstantBufferView> ConstantBufferView::create(const std::shared_ptr<BufferResource>& resource)
{
    (void)resource;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
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
    (void)resource;
    (void)format;
    (void)range;

    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return nullptr;
    case GraphicsApi::Vulkan:
        return nullptr;
    }
}

} // namespace Mizu
