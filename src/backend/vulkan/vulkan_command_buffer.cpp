#include "vulkan_command_buffer.h"

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_buffers.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_framebuffer.h"
#include "backend/vulkan/vulkan_graphics_pipeline.h"
#include "backend/vulkan/vulkan_queue.h"
#include "backend/vulkan/vulkan_render_pass.h"
#include "backend/vulkan/vulkan_resource_group.h"
#include "backend/vulkan/vulkan_shader.h"
#include "backend/vulkan/vulkan_synchronization.h"

namespace Mizu::Vulkan {

//
// VulkanCommandBufferBase
//

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::VulkanCommandBufferBase() {
    const auto cbs = VulkanContext.device->allocate_command_buffers(1, Type);
    assert(!cbs.empty() && "Error allocating command buffers");

    m_command_buffer = cbs[0];
}

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::~VulkanCommandBufferBase() {
    VulkanContext.device->free_command_buffers({m_command_buffer}, Type);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::begin_base() {
    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &info));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::end_base() {
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));

    m_bound_resources.clear();
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_base() const {
    submit_base({});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_base(const CommandBufferSubmitInfo& info) const {
    std::vector<VkSemaphore> wait_semaphores;
    if (info.wait_semaphore != nullptr) {
        const auto wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.wait_semaphore);
        wait_semaphores.push_back(wait_semaphore->handle());
    }

    std::vector<VkSemaphore> signal_semaphores;
    if (info.signal_semaphore != nullptr) {
        const auto signal_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.signal_semaphore);
        signal_semaphores.push_back(signal_semaphore->handle());
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 0;
    submit_info.pWaitSemaphores = nullptr;
    submit_info.pWaitDstStageMask = nullptr;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    VkFence signal_fence = info.signal_fence != nullptr
                               ? std::dynamic_pointer_cast<VulkanFence>(info.signal_fence)->handle()
                               : VK_NULL_HANDLE;

    get_queue()->submit(submit_info, signal_fence);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::bind_resource_group_base(const std::shared_ptr<ResourceGroup>& resource_group,
                                                             uint32_t set) {
    const auto native_rg = std::dynamic_pointer_cast<VulkanResourceGroup>(resource_group);
    m_bound_resources.insert({set, native_rg});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_single_time(
    const std::function<void(const VulkanCommandBufferBase<Type>&)>& func) {
    VulkanCommandBufferBase<Type> command_buffer{};

    command_buffer.begin_base();

    func(command_buffer);

    command_buffer.end_base();

    command_buffer.submit_base();

    vkQueueWaitIdle(get_queue()->handle());
}

template <CommandBufferType Type>
std::shared_ptr<VulkanQueue> VulkanCommandBufferBase<Type>::get_queue() {
    switch (Type) {
    case CommandBufferType::Graphics:
        return VulkanContext.device->get_graphics_queue();
    case CommandBufferType::Compute:
        return VulkanContext.device->get_compute_queue();
    case CommandBufferType::Transfer:
        return VulkanContext.device->get_transfer_queue();
    }
}

//
// VulkanRenderCommandBuffer
//

void VulkanRenderCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                    uint32_t set) {
    bind_resource_group_base(resource_group, set);

    if (m_bound_pipeline != nullptr) {
        bind_bound_resources(m_bound_pipeline->get_shader());
    }
}

void VulkanRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    m_bound_pipeline = std::dynamic_pointer_cast<VulkanGraphicsPipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bound_pipeline->handle());

    bind_bound_resources(m_bound_pipeline->get_shader());
}

void VulkanRenderCommandBuffer::begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    native_render_pass->begin(m_command_buffer);

    const auto target_framebuffer = native_render_pass->get_target_framebuffer();

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

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void VulkanRenderCommandBuffer::end_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    native_render_pass->end(m_command_buffer);
}

void VulkanRenderCommandBuffer::draw(const std::shared_ptr<VertexBuffer>& vertex) {
    const auto native_vertex = std::dynamic_pointer_cast<VulkanVertexBuffer>(vertex);
    native_vertex->bind(m_command_buffer);

    vkCmdDraw(m_command_buffer, native_vertex->count(), 1, 0, 0);
}

void VulkanRenderCommandBuffer::draw_indexed(const std::shared_ptr<VertexBuffer>& vertex,
                                             const std::shared_ptr<IndexBuffer>& index) {
    const auto native_vertex = std::dynamic_pointer_cast<VulkanVertexBuffer>(vertex);
    const auto native_index = std::dynamic_pointer_cast<VulkanIndexBuffer>(index);

    native_vertex->bind(m_command_buffer);
    native_index->bind(m_command_buffer);

    vkCmdDrawIndexed(m_command_buffer, native_index->count(), 1, 0, 0, 0);
}

void VulkanRenderCommandBuffer::bind_bound_resources(const std::shared_ptr<VulkanShader>& shader) const {
    std::vector<VkDescriptorSet> sets;
    for (const auto& [set, resource_group] : m_bound_resources) {
        if (!resource_group->is_baked()) {
            assert(resource_group->bake(shader, set) && "Could not bake bound resource group");
        }

        const VkDescriptorSet& descriptor_set = resource_group->get_descriptor_set();
        const VkPipelineLayout& pipeline_layout = shader->get_pipeline_layout();

        const auto& shader_layout = shader->get_descriptor_set_layout(set);
        if (!shader_layout.has_value()) {
            MIZU_LOG_ERROR("GraphicsPipeline being bound does not have descriptor set {} ", set);
            continue;
        }

        if (resource_group->get_descriptor_set_layout() != *shader_layout) {
            MIZU_LOG_ERROR("Layout of ResourceGroup in set {} does not match layout of GraphicsPipeline", set);
            continue;
        }

        vkCmdBindDescriptorSets(
            m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, set, 1, &descriptor_set, 0, nullptr);
    }
}

//
// Specializations
//

template class VulkanCommandBufferBase<CommandBufferType::Graphics>;
template class VulkanCommandBufferBase<CommandBufferType::Compute>;
template class VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
