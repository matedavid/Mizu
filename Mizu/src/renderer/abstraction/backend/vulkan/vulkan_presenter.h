#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/presenter.h"

// Forward declarations
namespace Mizu {
class Semaphore;
class GraphicsPipeline;
class ResourceGroup;
} // namespace Mizu

namespace Mizu::Vulkan {

// Forward declarations
class VulkanRenderPass;
class VulkanSwapchain;
class VulkanRenderCommandBuffer;
class VulkanTexture2D;

class VulkanPresenter : public Presenter {
  public:
    VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~VulkanPresenter() override;

    void present() override;
    void present(const std::shared_ptr<Semaphore>& wait_semaphore) override;

    void window_resized(uint32_t width, uint32_t height) override;

  private:
    std::shared_ptr<Window> m_window;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    std::shared_ptr<VulkanTexture2D> m_present_texture;

    std::shared_ptr<GraphicsPipeline> m_present_pipeline;
    std::unique_ptr<VulkanRenderPass> m_present_render_pass;
    std::shared_ptr<ResourceGroup> m_present_resources;

    std::unique_ptr<VulkanRenderCommandBuffer> m_command_buffer;

    VkSemaphore m_image_available_semaphore;
    VkSemaphore m_rendering_finished_semaphore;
    VkFence m_present_fence;

    void init(uint32_t width, uint32_t height);
};

} // namespace Mizu::Vulkan
