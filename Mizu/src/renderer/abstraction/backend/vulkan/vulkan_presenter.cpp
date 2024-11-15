#include "vulkan_presenter.h"

#include <array>

#include "core/window.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_framebuffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"
#include "renderer/abstraction/backend/vulkan/vulkan_queue.h"
#include "renderer/abstraction/backend/vulkan/vulkan_render_pass.h"
#include "renderer/abstraction/backend/vulkan/vulkan_resource_group.h"
#include "renderer/abstraction/backend/vulkan/vulkan_swapchain.h"
#include "renderer/abstraction/backend/vulkan/vulkan_synchronization.h"

#include "managers/shader_manager.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanPresenter::VulkanPresenter(std::shared_ptr<Window> window, std::shared_ptr<Texture2D> texture)
      : m_window(std::move(window)) {
    m_present_texture = std::dynamic_pointer_cast<VulkanImageResource>(texture->get_resource());

    VK_CHECK(m_window->create_vulkan_surface(VulkanContext.instance->handle(), m_surface));
    m_swapchain = std::make_unique<VulkanSwapchain>(m_surface, m_window);

    init();

    // Synchronization
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

    const std::vector<PresenterVertex> vertex_data = {
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 1.0f})},
        {.position = glm::vec3(1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 1.0f})},
        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 0.0f})},

        {.position = glm::vec3(1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({1.0f, 0.0f})},
        {.position = glm::vec3(-1.0f, -1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 0.0f})},
        {.position = glm::vec3(-1.0f, 1.0f, 0.0f), .texture_coords = convert_texture_coords({0.0f, 1.0f})},
    };

    std::vector<VertexBuffer::Layout> vertex_layout = {
        {.type = VertexBuffer::Layout::Type::Float, .count = 3, .normalized = false},
        {.type = VertexBuffer::Layout::Type::Float, .count = 2, .normalized = false},
    };

    m_vertex_buffer = Mizu::VertexBuffer::create(vertex_data, vertex_layout);
}

VulkanPresenter::~VulkanPresenter() {
    vkDeviceWaitIdle(VulkanContext.device->handle());

    m_swapchain.reset();

    vkDestroySemaphore(VulkanContext.device->handle(), m_image_available_semaphore, nullptr);
    vkDestroySemaphore(VulkanContext.device->handle(), m_rendering_finished_semaphore, nullptr);
    vkDestroyFence(VulkanContext.device->handle(), m_present_fence, nullptr);

    vkDestroySurfaceKHR(VulkanContext.instance->handle(), m_surface, nullptr);
}

void VulkanPresenter::present() {
    present(nullptr);
}

void VulkanPresenter::present(const std::shared_ptr<Semaphore>& wait_semaphore) {
    VK_CHECK(vkWaitForFences(VulkanContext.device->handle(), 1, &m_present_fence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(VulkanContext.device->handle(), 1, &m_present_fence));

    m_swapchain->acquire_next_image(m_image_available_semaphore, VK_NULL_HANDLE);

    const auto& target_framebuffer = m_swapchain->get_current_framebuffer();

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(target_framebuffer->get_width());
    viewport.height = static_cast<float>(target_framebuffer->get_height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {target_framebuffer->get_width(), target_framebuffer->get_height()};

    m_command_buffer->begin();
    {
        VK_DEBUG_BEGIN_LABEL(m_command_buffer->handle(), "Presentation");

        m_command_buffer->begin_render_pass(m_present_render_pass, target_framebuffer);

        m_command_buffer->bind_pipeline(m_present_pipeline);
        m_command_buffer->bind_resource_group(m_present_resources, 0);

        vkCmdSetViewport(m_command_buffer->handle(), 0, 1, &viewport);
        vkCmdSetScissor(m_command_buffer->handle(), 0, 1, &scissor);

        m_command_buffer->draw(m_vertex_buffer);

        m_command_buffer->end_render_pass(std::dynamic_pointer_cast<RenderPass>(m_present_render_pass));

        VK_DEBUG_END_LABEL(m_command_buffer->handle());
    }
    m_command_buffer->end();

    const std::array<VkCommandBuffer, 1> command_buffers = {m_command_buffer->handle()};

    std::vector<VkSemaphore> wait_semaphores = {m_image_available_semaphore};
    std::vector<VkPipelineStageFlags> wait_stages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    if (wait_semaphore != nullptr) {
        const auto& native_wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(wait_semaphore);
        wait_semaphores.push_back(native_wait_semaphore->handle());
        wait_stages.push_back(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
    }

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
    const std::vector<VkSemaphore> present_wait_semaphores = {
        m_rendering_finished_semaphore,
    };

    const VkSwapchainKHR& swapchain = m_swapchain->handle();
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
        MIZU_UNREACHABLE("Failed to present image");
    }
}

void VulkanPresenter::texture_changed(std::shared_ptr<Texture2D> texture) {
    m_present_texture = std::dynamic_pointer_cast<VulkanImageResource>(texture->get_resource());
    MIZU_ASSERT(m_present_texture != nullptr, "Texture cannot be nullptr");

    vkQueueWaitIdle(VulkanContext.device->get_graphics_queue()->handle());

    m_swapchain->recreate();
    init();
}

void VulkanPresenter::init() {
    m_present_pipeline = std::make_shared<VulkanGraphicsPipeline>(GraphicsPipeline::Description{
        .shader = ShaderManager::get_shader({"/EngineShaders/presenter/present.vert.spv", "main"},
                                            {"/EngineShaders/presenter/present.frag.spv", "main"}),
        .target_framebuffer = m_swapchain->get_target_framebuffer(),
        .depth_stencil = {.depth_test = false},
    });

    m_present_render_pass = std::make_shared<VulkanRenderPass>(RenderPass::Description{
        .target_framebuffer = m_swapchain->get_target_framebuffer(),
    });

    m_present_resources = std::make_shared<VulkanResourceGroup>();
    m_present_resources->add_resource("uPresentTexture", std::dynamic_pointer_cast<ImageResource>(m_present_texture));

    m_command_buffer = std::make_unique<VulkanRenderCommandBuffer>();
}

} // namespace Mizu::Vulkan
