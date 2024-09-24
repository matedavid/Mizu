#include "vulkan_command_buffer.h"

#include "utility/assert.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_compute_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_framebuffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_graphics_pipeline.h"
#include "renderer/abstraction/backend/vulkan/vulkan_queue.h"
#include "renderer/abstraction/backend/vulkan/vulkan_render_pass.h"
#include "renderer/abstraction/backend/vulkan/vulkan_resource_group.h"
#include "renderer/abstraction/backend/vulkan/vulkan_shader.h"
#include "renderer/abstraction/backend/vulkan/vulkan_synchronization.h"

namespace Mizu::Vulkan {

//
// VulkanCommandBufferBase
//

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::VulkanCommandBufferBase() {
    const auto cbs = VulkanContext.device->allocate_command_buffers(1, Type);
    MIZU_ASSERT(!cbs.empty(), "Error allocating command buffers");

    m_command_buffer = cbs[0];
}

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::~VulkanCommandBufferBase() {
    VulkanContext.device->free_command_buffers({m_command_buffer}, Type);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::begin() {
    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &info));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::end() {
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));

    m_bound_resources.clear();
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit() const {
    submit({});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit(const CommandBufferSubmitInfo& info) const {
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
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
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
void VulkanCommandBufferBase<Type>::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                        uint32_t set) {
    const auto native_rg = std::dynamic_pointer_cast<VulkanResourceGroup>(resource_group);
    m_bound_resources.insert({set, native_rg});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_single_time(
    const std::function<void(const VulkanCommandBufferBase<Type>&)>& func) {
    VulkanCommandBufferBase<Type> command_buffer{};

    command_buffer.begin();

    func(command_buffer);

    command_buffer.end();

    command_buffer.submit();

    vkQueueWaitIdle(get_queue()->handle());
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::bind_bound_resources(const std::shared_ptr<VulkanShaderBase>& shader,
                                                         VkPipelineBindPoint bind_point) const {
    std::vector<VkDescriptorSet> sets;
    for (const auto& [set, resource_group] : m_bound_resources) {
        if (!resource_group->is_baked()) {
            [[maybe_unused]] const bool baked = resource_group->bake(shader, set);
            MIZU_ASSERT(baked, "Could not bake bound resource group");
        }

        const VkDescriptorSet& descriptor_set = resource_group->get_descriptor_set();
        const VkPipelineLayout& pipeline_layout = shader->get_pipeline_layout();

        const auto& shader_layout = shader->get_descriptor_set_layout(set);
        if (!shader_layout.has_value()) {
            MIZU_LOG_ERROR("GraphicsPipeline being bound does not have descriptor set {} ", set);
            continue;
        }

        if (resource_group->get_descriptor_set_layout() != *shader_layout) {
            MIZU_LOG_ERROR("Layout of ResourceGroup in set {} does not match layout of Pipeline", set);
            continue;
        }

        vkCmdBindDescriptorSets(m_command_buffer, bind_point, pipeline_layout, set, 1, &descriptor_set, 0, nullptr);
    }
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
    VulkanCommandBufferBase<CommandBufferType::Graphics>::bind_resource_group(resource_group, set);

    if (m_bound_graphics_pipeline != nullptr) {
        bind_bound_resources(m_bound_graphics_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_GRAPHICS);
    } else if (m_bound_compute_pipeline != nullptr) {
        bind_bound_resources(m_bound_compute_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_COMPUTE);
    }
}

void VulkanRenderCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) {
    if (m_bound_graphics_pipeline != nullptr) {
        m_bound_graphics_pipeline->push_constant(m_command_buffer, name, size, data);
    } else if (m_bound_compute_pipeline != nullptr) {
        m_bound_compute_pipeline->push_constant(m_command_buffer, name, size, data);
    } else {
        MIZU_LOG_WARNING("Can't push constant because no Pipeline has been bound");
    }
}

void VulkanRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    m_bound_resources.clear();

    m_bound_graphics_pipeline = std::dynamic_pointer_cast<VulkanGraphicsPipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bound_graphics_pipeline->handle());

    bind_bound_resources(m_bound_graphics_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_GRAPHICS);

    m_bound_compute_pipeline = nullptr;
}

void VulkanRenderCommandBuffer::bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) {
    m_bound_resources.clear();

    m_bound_compute_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bound_compute_pipeline->handle());

    bind_bound_resources(m_bound_compute_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_COMPUTE);

    m_bound_graphics_pipeline = nullptr;
}

void VulkanRenderCommandBuffer::begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    begin_render_pass(native_render_pass, native_render_pass->get_target_framebuffer());
}

void VulkanRenderCommandBuffer::begin_render_pass(const std::shared_ptr<VulkanRenderPass>& render_pass,
                                                  const std::shared_ptr<VulkanFramebuffer>& framebuffer) {
    render_pass->begin(m_command_buffer, framebuffer->handle());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(framebuffer->get_width());
    viewport.height = static_cast<float>(framebuffer->get_height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {framebuffer->get_width(), framebuffer->get_height()};

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void VulkanRenderCommandBuffer::end_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    native_render_pass->end(m_command_buffer);
}

void VulkanRenderCommandBuffer::draw(const std::shared_ptr<VertexBuffer>& vertex) {
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr,
                "To call draw on RenderCommandBuffer you must have previously bound a GraphicsPipeline");

    const auto native_vertex = std::dynamic_pointer_cast<VulkanVertexBuffer>(vertex);
    native_vertex->bind(m_command_buffer);

    vkCmdDraw(m_command_buffer, native_vertex->count(), 1, 0, 0);
}

void VulkanRenderCommandBuffer::draw_indexed(const std::shared_ptr<VertexBuffer>& vertex,
                                             const std::shared_ptr<IndexBuffer>& index) {
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr,
                "To call draw_indexed on RenderCommandBuffer you must have previously bound a GraphicsPipeline");

    const auto native_vertex = std::dynamic_pointer_cast<VulkanVertexBuffer>(vertex);
    const auto native_index = std::dynamic_pointer_cast<VulkanIndexBuffer>(index);

    native_vertex->bind(m_command_buffer);
    native_index->bind(m_command_buffer);

    vkCmdDrawIndexed(m_command_buffer, native_index->count(), 1, 0, 0, 0);
}

void VulkanRenderCommandBuffer::dispatch(glm::uvec3 group_count) {
    MIZU_ASSERT(m_bound_compute_pipeline != nullptr,
                "To call dispatch on RenderCommandBuffer you must have previously bound a ComputePipeline");

    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

//
// VulkanComputeCommandBuffer
//

void VulkanComputeCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                     uint32_t set) {
    VulkanCommandBufferBase<CommandBufferType::Compute>::bind_resource_group(resource_group, set);

    if (m_bound_pipeline != nullptr) {
        bind_bound_resources(m_bound_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_COMPUTE);
    }
}

void VulkanComputeCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) {
    if (m_bound_pipeline == nullptr) {
        MIZU_LOG_WARNING("Can't push constant because no GraphicsPipeline has been bound");
        return;
    }

    m_bound_pipeline->push_constant(m_command_buffer, name, size, data);
}

void VulkanComputeCommandBuffer::bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) {
    m_bound_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bound_pipeline->handle());

    bind_bound_resources(m_bound_pipeline->get_shader(), VK_PIPELINE_BIND_POINT_COMPUTE);
}

void VulkanComputeCommandBuffer::dispatch(glm::uvec3 group_count) {
    MIZU_ASSERT(m_bound_pipeline != nullptr, "To call dispatch you must have previously bound a ComputePipeline");

    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

//
// Specializations
//

template class VulkanCommandBufferBase<CommandBufferType::Graphics>;
template class VulkanCommandBufferBase<CommandBufferType::Compute>;
template class VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
