#pragma once

#include <memory>
#include <vector>

#include "render_core/rhi/swapchain.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;

class VulkanSwapchain : public Swapchain
{
  public:
    VulkanSwapchain(SwapchainDescription desc);
    ~VulkanSwapchain() override;

    void acquire_next_image(std::shared_ptr<Semaphore> signal_semaphore, std::shared_ptr<Fence> signal_fence) override;
    void present(const std::vector<std::shared_ptr<Semaphore>>& wait_semaphores) override;
    std::shared_ptr<ImageResource> get_image(uint32_t idx) const override;
    uint32_t get_current_image_idx() const override { return m_current_image_idx; }

    VkSurfaceKHR get_surface() const { return m_surface; }
    VkSwapchainKHR handle() const { return m_swapchain; }

  private:
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    uint32_t m_current_image_idx = 0;

    SwapchainDescription m_description{};

    struct SwapchainInformation
    {
        VkSurfaceCapabilitiesKHR capabilities;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkExtent2D extent;
    };
    SwapchainInformation m_swapchain_info{};

    std::vector<std::shared_ptr<VulkanImageResource>> m_images;

    void retrieve_surface();
    void create_swapchain();
    void retrieve_swapchain_images();

    void recreate();
    void cleanup();

    void retrieve_swapchain_information();
};

} // namespace Mizu::Vulkan
