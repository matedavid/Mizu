#include "synchronization.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

namespace Mizu
{

std::shared_ptr<Fence> Fence::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanFence>();
    }
}

std::shared_ptr<Semaphore> Semaphore::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSemaphore>();
    }
}

} // namespace Mizu
