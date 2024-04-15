#include "framebuffer.h"

#include "renderer.h"

#include "backend/vulkan/vulkan_framebuffer.h"

namespace Mizu {

std::shared_ptr<Framebuffer> Framebuffer::create(const Description& desc) {
    switch (get_config().graphics_api) {
    default:
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanFramebuffer>(desc);
    }
}

} // namespace Mizu
