#include "swapchain.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_swapchain.h"
#include "render_core/rhi/backend/vulkan/vulkan_swapchain.h"

namespace Mizu
{

std::shared_ptr<Swapchain> Swapchain::create(const SwapchainDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::DirectX12:
        return std::make_shared<Dx12::Dx12Swapchain>(desc);
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSwapchain>(desc);
    }
}

} // namespace Mizu