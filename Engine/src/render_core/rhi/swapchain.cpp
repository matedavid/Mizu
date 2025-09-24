#include "swapchain.h"

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vulkan_swapchain.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

std::shared_ptr<Swapchain> Swapchain::create(std::shared_ptr<IRHIWindow> window)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSwapchain>(std::move(window));
    case GraphicsAPI::OpenGL:
        MIZU_UNREACHABLE("Unimplemented");
        return nullptr;
    }
}

} // namespace Mizu