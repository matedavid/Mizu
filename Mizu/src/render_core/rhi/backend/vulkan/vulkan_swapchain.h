#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

// Forward declarations
namespace Mizu
{

class Window;
class Texture2D;

} // namespace Mizu

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanFramebuffer;
class VulkanImageResource;

class VulkanSwapchain
{
  public:
    explicit VulkanSwapchain(VkSurfaceKHR surface, std::shared_ptr<Window> window);
    ~VulkanSwapchain();

    void acquire_next_image(VkSemaphore signal_semaphore, VkFence signal_fence);
    void recreate();

    [[nodiscard]] uint32_t get_current_image_idx() const { return m_current_image_idx; }

    [[nodiscard]] std::shared_ptr<VulkanFramebuffer> get_target_framebuffer() const { return m_framebuffers[0]; }
    [[nodiscard]] std::shared_ptr<VulkanFramebuffer> get_current_framebuffer() const
    {
        return m_framebuffers[m_current_image_idx];
    }

    [[nodiscard]] VkSwapchainKHR handle() const { return m_swapchain; }

  private:
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    uint32_t m_current_image_idx = 0;

    std::shared_ptr<Window> m_window;

    struct SwapchainInformation
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkExtent2D extent;
    };
    SwapchainInformation m_swapchain_info{};

    std::vector<VkImageView> m_image_views;
    std::vector<std::shared_ptr<VulkanImageResource>> m_images;

    std::shared_ptr<Texture2D> m_depth_image;

    VkRenderPass m_render_pass;
    std::vector<std::shared_ptr<VulkanFramebuffer>> m_framebuffers;

    void create_swapchain();
    void retrieve_swapchain_images();
    void create_render_pass();
    void create_framebuffers();

    void cleanup();

    void retrieve_swapchain_information();
};

} // namespace Mizu::Vulkan
