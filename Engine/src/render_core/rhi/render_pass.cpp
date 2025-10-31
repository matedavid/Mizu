#include "render_pass.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_render_pass.h"
#include "render_core/rhi/backend/vulkan/vulkan_render_pass.h"

namespace Mizu
{

std::shared_ptr<RenderPass> RenderPass::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12RenderPass>(desc);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderPass>(desc);
    }
}

} // namespace Mizu
