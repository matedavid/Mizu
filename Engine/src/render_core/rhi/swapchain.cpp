#include "swapchain.h"

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_swapchain.h"

namespace Mizu
{

std::shared_ptr<Swapchain> Swapchain::create(std::shared_ptr<IRHIWindow> window)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSwapchain>(std::move(window));
    }
}

} // namespace Mizu