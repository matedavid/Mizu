#pragma once

#include <vulkan/vulkan.h>

#include "renderer/abstraction/presenter.h"

// Forward declarations
namespace Mizu
{

class Semaphore;
class GraphicsPipeline;
class ResourceGroup;
class VertexBuffer;
class ImageResource;

} // namespace Mizu

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanRenderPass;
class VulkanSwapchain;
class VulkanRenderCommandBuffer;
class VulkanImageResource;

class VulkanPresenter : public Presenter
{
  public:
    VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~VulkanPresenter() override;

    void present() override;
    void present(const std::shared_ptr<Semaphore>& wait_semaphore) override;

    void texture_changed(std::shared_ptr<Texture2D> texture) override;

  private:
    std::shared_ptr<Window> m_window;
    std::unique_ptr<VulkanSwapchain> m_swapchain;
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};

    std::shared_ptr<VulkanImageResource> m_present_texture;

    std::shared_ptr<GraphicsPipeline> m_present_pipeline;
    std::shared_ptr<VulkanRenderPass> m_present_render_pass;
    std::shared_ptr<ResourceGroup> m_present_resources;

    std::unique_ptr<VulkanRenderCommandBuffer> m_command_buffer;

    VkSemaphore m_image_available_semaphore{VK_NULL_HANDLE};
    VkSemaphore m_rendering_finished_semaphore{VK_NULL_HANDLE};
    VkFence m_present_fence{VK_NULL_HANDLE};

    std::shared_ptr<VertexBuffer> m_vertex_buffer;

    void init();
};

} // namespace Mizu::Vulkan
