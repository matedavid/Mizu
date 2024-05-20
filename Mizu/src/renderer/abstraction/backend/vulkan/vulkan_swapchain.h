#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

namespace Mizu::Vulkan {

// Forward declarations
class VulkanImage;
class VulkanFramebuffer;

class VulkanSwapchain {
  public:
    explicit VulkanSwapchain(VkSurfaceKHR surface, uint32_t width, uint32_t height);
    ~VulkanSwapchain();

    void recreate(uint32_t width, uint32_t height);

  private:
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    uint32_t m_width, m_height;
};

} // namespace Mizu::Vulkan
