#include "vulkan_presenter.h"

#include "core/window.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_render_pass.h"
#include "renderer/abstraction/backend/vulkan/vulkan_resource_group.h"
#include "renderer/abstraction/backend/vulkan/vulkan_swapchain.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

VulkanPresenter::VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)), m_present_texture(std::move(texture)) {
    VK_CHECK(m_window->create_vulkan_surface(VulkanContext.instance->handle(), m_surface));
    m_swapchain = std::make_unique<VulkanSwapchain>(m_surface, m_window);
}

VulkanPresenter::~VulkanPresenter() {
    m_swapchain.reset();
    vkDestroySurfaceKHR(VulkanContext.instance->handle(), m_surface, nullptr);
}

void VulkanPresenter::present() {}

void VulkanPresenter::window_resized(uint32_t width, uint32_t height) {
    // TODO:
}

void VulkanPresenter::init(uint32_t width, uint32_t height) {
    m_present_pipeline = std::make_unique<VulkanGraphicsPipeline>(GraphicsPipeline::Description{
        .shader = nullptr,
        .target_framebuffer = nullptr,
    });

    m_present_resources = std::make_unique<VulkanResourceGroup>();
    m_present_resources->add_resource("uTexture", m_present_texture);
}

} // namespace Mizu::Vulkan
