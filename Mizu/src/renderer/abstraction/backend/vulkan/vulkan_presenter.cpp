#include "vulkan_presenter.h"

#include "core/window.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_framebuffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_queue.h"
#include "renderer/abstraction/backend/vulkan/vulkan_render_pass.h"
#include "renderer/abstraction/backend/vulkan/vulkan_resource_group.h"
#include "renderer/abstraction/backend/vulkan/vulkan_shader.h"
#include "renderer/abstraction/backend/vulkan/vulkan_swapchain.h"
#include "renderer/abstraction/backend/vulkan/vulkan_synchronization.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

VulkanPresenter::VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)) {
    m_present_texture = std::dynamic_pointer_cast<VulkanTexture2D>(texture);

    VK_CHECK(m_window->create_vulkan_surface(VulkanContext.instance->handle(), m_surface));
    m_swapchain = std::make_unique<VulkanSwapchain>(m_surface, m_window);

    init(m_present_texture->get_width(), m_present_texture->get_height());

    // Syncronization
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateSemaphore(
        VulkanContext.device->handle(), &semaphore_create_info, nullptr, &m_image_available_semaphore));
    VK_CHECK(vkCreateSemaphore(
        VulkanContext.device->handle(), &semaphore_create_info, nullptr, &m_rendering_finished_semaphore));
    VK_CHECK(vkCreateFence(VulkanContext.device->handle(), &fence_create_info, nullptr, &m_present_fence));
}

VulkanPresenter::~VulkanPresenter() {
    m_swapchain.reset();

    vkDestroySemaphore(VulkanContext.device->handle(), m_image_available_semaphore, nullptr);
    vkDestroySemaphore(VulkanContext.device->handle(), m_rendering_finished_semaphore, nullptr);
    vkDestroyFence(VulkanContext.device->handle(), m_present_fence, nullptr);

    vkDestroySurfaceKHR(VulkanContext.instance->handle(), m_surface, nullptr);
}

void VulkanPresenter::present(const std::shared_ptr<Semaphore>& wait_semaphore) {
    const auto& native_wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(wait_semaphore);

    m_swapchain->acquire_next_image(m_image_available_semaphore, VK_NULL_HANDLE);

    m_command_buffer->begin();
    {}
    m_command_buffer->end();

    const std::array<VkPipelineStageFlags, 1> wait_stages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const std::array<VkCommandBuffer, 1> command_buffers = {m_command_buffer->handle()};
    const std::array<VkSemaphore, 2> wait_semaphores = {m_image_available_semaphore, native_wait_semaphore->handle()};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = static_cast<uint32_t>(command_buffers.size());
    submit_info.pCommandBuffers = command_buffers.data();
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &m_rendering_finished_semaphore;

    const auto& graphics_queue = VulkanContext.device->get_graphics_queue();
    graphics_queue->submit(submit_info, m_present_fence);

    // Present image
    const std::vector<VkSemaphore> present_wait_semaphores = {m_rendering_finished_semaphore,
                                                              native_wait_semaphore->handle()};

    const VkSwapchainKHR swapchain = m_swapchain->handle();
    const uint32_t swapchain_current_image_idx = m_swapchain->get_current_image_idx();

    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    info.waitSemaphoreCount = static_cast<uint32_t>(present_wait_semaphores.size());
    info.pWaitSemaphores = present_wait_semaphores.data();
    info.swapchainCount = 1;
    info.pSwapchains = &swapchain;
    info.pImageIndices = &swapchain_current_image_idx;

    const VkResult result = vkQueuePresentKHR(graphics_queue->handle(), &info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_swapchain->recreate();
    } else if (result != VK_SUCCESS) {
        assert(false && "Failed to present image");
    }
}

void VulkanPresenter::window_resized(uint32_t width, uint32_t height) {
    // TODO:
}

void VulkanPresenter::init(uint32_t width, uint32_t height) {
    m_present_pipeline = std::make_unique<VulkanGraphicsPipeline>(GraphicsPipeline::Description{
        .shader = Shader::create("../Mizu/shaders/present.vert.spv", "../Mizu/shaders/present.frag.spv"),
        .target_framebuffer = nullptr,
    });

    m_present_resources = std::make_unique<VulkanResourceGroup>();
    m_present_resources->add_resource("uTexture", m_present_texture);

    m_command_buffer = std::make_unique<VulkanRenderCommandBuffer>();
}

} // namespace Mizu::Vulkan
