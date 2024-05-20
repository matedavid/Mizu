#include "vulkan_swapchain.h"

namespace Mizu::Vulkan {

VulkanSwapchain::VulkanSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height)
      : m_surface(surface), m_width(width), m_height(height) {
    // TODO:
}

VulkanSwapchain::~VulkanSwapchain() {
    // TODO:
}

void VulkanSwapchain::recreate(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;

    // TODO:
}

} // namespace Mizu::Vulkan
