#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations
namespace Mizu {
class Window;
}

namespace Mizu::Vulkan {

// Forward declarations
class VulkanImage;

class VulkanSwapchain {
  public:
    explicit VulkanSwapchain(VkSurfaceKHR surface, std::shared_ptr<Window> window);
    ~VulkanSwapchain();

    void acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence);
    void recreate();

    [[nodiscard]] uint32_t get_current_image_idx() const { return m_current_image_idx; }

    [[nodiscard]] VkSwapchainKHR handle() const { return m_swapchain; }

  private:
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    uint32_t m_current_image_idx = 0;

    std::shared_ptr<Window> m_window;

    struct SwapchainInformation {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkExtent2D extent;
    };
    SwapchainInformation m_swapchain_info{};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_image_views;

    std::unique_ptr<VulkanImage> m_depth_image;

    VkRenderPass m_render_pass;
    std::vector<VkFramebuffer> m_framebuffers;

    void create_swapchain();
    void retrieve_swapchain_images();
    void create_render_pass();
    void create_framebuffers();

    void cleanup();

    void retrieve_swapchain_information();
};

} // namespace Mizu::Vulkan
