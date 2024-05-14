#pragma once

#include "renderer/abstraction/presenter.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanGraphicsPipeline;
class VulkanRenderPass;
class VulkanResourceGroup;

class VulkanPresenter : public Presenter {
  public:
    VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture);
    ~VulkanPresenter() override;

    void present() override;
    void window_resized(uint32_t width, uint32_t height) override;

  private:
    std::shared_ptr<Window> m_window;
    std::shared_ptr<Texture2D> m_present_texture;

    std::unique_ptr<VulkanGraphicsPipeline> m_present_pipeline;
    std::unique_ptr<VulkanRenderPass> m_present_render_pass;
    std::unique_ptr<VulkanResourceGroup> m_present_resources;

    void init(uint32_t width, uint32_t height);
};

} // namespace Mizu::Vulkan
