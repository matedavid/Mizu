#include "graphics_pipeline.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_graphics_pipeline.h"
#include "render_core/rhi/backend/vulkan/vulkan_graphics_pipeline.h"

namespace Mizu
{

std::shared_ptr<GraphicsPipeline> GraphicsPipeline::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12GraphicsPipeline>(desc);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanGraphicsPipeline>(desc);
    }
}

} // namespace Mizu
