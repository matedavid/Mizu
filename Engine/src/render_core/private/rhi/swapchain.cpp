#include "render_core/rhi/swapchain.h"

#include "backend/dx12/dx12_swapchain.h"
#include "backend/vulkan/vulkan_swapchain.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Swapchain> Swapchain::create(const SwapchainDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12Swapchain>(desc);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanSwapchain>(desc);
    }
}

} // namespace Mizu