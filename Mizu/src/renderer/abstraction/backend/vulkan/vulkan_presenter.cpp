#include "vulkan_presenter.h"

#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_render_pass.h"
#include "renderer/abstraction/backend/vulkan/vulkan_resource_group.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

VulkanPresenter::VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)), m_present_texture(texture) {}

VulkanPresenter::~VulkanPresenter() {}

void VulkanPresenter::present() {}

void VulkanPresenter::window_resized(uint32_t width, uint32_t height) {}

void VulkanPresenter::init(uint32_t width, uint32_t height) {
    m_present_pipeline = std::make_unique<VulkanGraphicsPipeline>(GraphicsPipeline::Description{
        .shader = nullptr,
        .target_framebuffer = nullptr,
    });

    m_present_resources = std::make_unique<VulkanResourceGroup>();
    m_present_resources->add_resource("uTexture", m_present_texture);
}

} // namespace Mizu::Vulkan
