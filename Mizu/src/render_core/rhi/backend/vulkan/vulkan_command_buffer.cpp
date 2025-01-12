#include "vulkan_command_buffer.h"

#include <array>

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_compute_pipeline.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_graphics_pipeline.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_queue.h"
#include "render_core/rhi/backend/vulkan/vulkan_render_pass.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_group.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu::Vulkan
{

//
// VulkanCommandBufferBase
//

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::VulkanCommandBufferBase()
{
    const auto cbs = VulkanContext.device->allocate_command_buffers(1, Type);
    MIZU_ASSERT(!cbs.empty(), "Error allocating command buffers");

    m_command_buffer = cbs[0];
}

template <CommandBufferType Type>
VulkanCommandBufferBase<Type>::~VulkanCommandBufferBase()
{
    VulkanContext.device->free_command_buffers({m_command_buffer}, Type);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::begin()
{
    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &info));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::end()
{
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit() const
{
    submit({});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit(const CommandBufferSubmitInfo& info) const
{
    std::vector<VkSemaphore> wait_semaphores;
    if (info.wait_semaphore != nullptr)
    {
        const auto wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.wait_semaphore);
        wait_semaphores.push_back(wait_semaphore->handle());
    }

    std::vector<VkSemaphore> signal_semaphores;
    if (info.signal_semaphore != nullptr)
    {
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
void VulkanCommandBufferBase<Type>::bind_resource_group(const ResourceGroup& resource_group, uint32_t set) const
{
    MIZU_ASSERT(m_currently_bound_shader != nullptr, "Can't bind resource group because no pipeline has been bound");

    const VulkanResourceGroup& native_resource_group = dynamic_cast<const VulkanResourceGroup&>(resource_group);

    const VkDescriptorSet& descriptor_set = native_resource_group.get_descriptor_set();
    const VkPipelineLayout& pipeline_layout = m_currently_bound_shader->get_pipeline_layout();

    const auto& shader_layout = m_currently_bound_shader->get_descriptor_set_layout(set);
    if (!shader_layout.has_value())
    {
        MIZU_LOG_ERROR("GraphicsPipeline being bound does not have descriptor set {} ", set);
        return;
    }

    if (native_resource_group.get_descriptor_set_layout() != *shader_layout)
    {
        MIZU_LOG_ERROR("Layout of ResourceGroup in set {} does not match layout of Pipeline", set);
        return;
    }

    VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_MAX_ENUM;
    switch (Type)
    {
    case CommandBufferType::Graphics:
        bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
        break;
    case CommandBufferType::Compute:
        bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;
        break;
    default:
        MIZU_UNREACHABLE("Not valid CommandBuffer Type")
    }

    MIZU_ASSERT(bind_point != VK_PIPELINE_BIND_POINT_MAX_ENUM, "Bind point not valid");

    vkCmdBindDescriptorSets(m_command_buffer, bind_point, pipeline_layout, set, 1, &descriptor_set, 0, nullptr);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    MIZU_ASSERT(m_currently_bound_shader != nullptr, "No pipeline has been bound");

    const auto info = m_currently_bound_shader->get_constant(name);
    MIZU_ASSERT(info.has_value(), "Push constant '{}' not found in shader", name);

    MIZU_ASSERT(info->size == size,
                "Size of provided data and size of push constant do not match ({} != {})",
                size,
                info->size);

    vkCmdPushConstants(m_command_buffer,
                       m_currently_bound_shader->get_pipeline_layout(),
                       *m_currently_bound_shader->get_constant_stage(name),
                       0,
                       size,
                       data);
}

struct TransitionInfo
{
    VkPipelineStageFlags src_stage, dst_stage;
    VkAccessFlags src_access_mask, dst_access_mask;
};

#if MIZU_DEBUG

static std::string to_string(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return "Undefined";
    case ImageResourceState::General:
        return "General";
    case ImageResourceState::TransferDst:
        return "TransferDst";
    case ImageResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    case ImageResourceState::ColorAttachment:
        return "ColorAttachment";
    case ImageResourceState::DepthStencilAttachment:
        return "DepthStencilAttachment";
    }
}

#endif

#define DEFINE_TRANSITION(oldl, newl, src_mask, dst_mask, src_stage, dst_stage) \
    {                                                                           \
        {ImageResourceState::oldl, ImageResourceState::newl}, TransitionInfo    \
        {                                                                       \
            src_stage, dst_stage, src_mask, dst_mask                            \
        }                                                                       \
    }

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::transition_resource(ImageResource& image,
                                                        ImageResourceState old_state,
                                                        ImageResourceState new_state) const
{
    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    auto& native_image = dynamic_cast<VulkanImageResource&>(image);

    const VkImageLayout old_layout = VulkanImageResource::get_vulkan_image_resource_state(old_state);
    const VkImageLayout new_layout = VulkanImageResource::get_vulkan_image_resource_state(new_state);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = native_image.get_image_handle();
    barrier.subresourceRange.aspectMask =
        ImageUtils::is_depth_format(image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = native_image.get_num_mips();
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = native_image.get_num_layers();

    // NOTE: At the moment only specifying "expected transitions"
    static std::map<std::pair<ImageResourceState, ImageResourceState>, TransitionInfo> s_transition_info{
        DEFINE_TRANSITION(Undefined,
                          TransferDst,
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT),
        DEFINE_TRANSITION(Undefined,
                          General,
                          0,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
        DEFINE_TRANSITION(Undefined,
                          ColorAttachment,
                          0,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
        DEFINE_TRANSITION(Undefined,
                          DepthStencilAttachment,
                          0,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        DEFINE_TRANSITION(General,
                          ShaderReadOnly,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        DEFINE_TRANSITION(TransferDst,
                          ShaderReadOnly,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        DEFINE_TRANSITION(ColorAttachment,
                          ShaderReadOnly,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        DEFINE_TRANSITION(DepthStencilAttachment,
                          ShaderReadOnly,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // TODO: Not sure this is correct
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
    };

    const auto it = s_transition_info.find({old_state, new_state});
    if (it == s_transition_info.end())
    {
        MIZU_LOG_ERROR("Image layout transition not defined: {} -> {}", to_string(old_state), to_string(new_state));
        return;
    }

    const TransitionInfo& info = it->second;

    barrier.srcAccessMask = info.src_access_mask;
    barrier.dstAccessMask = info.dst_access_mask;

    vkCmdPipelineBarrier(m_command_buffer, info.src_stage, info.dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // TODO: native_image.set_current_state(new_state);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::begin_debug_label(const std::string_view& label) const
{
    VK_DEBUG_BEGIN_LABEL(m_command_buffer, label);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::end_debug_label() const
{
    VK_DEBUG_END_LABEL(m_command_buffer);
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::submit_single_time(
    const std::function<void(const VulkanCommandBufferBase<Type>&)>& func)
{
    VulkanCommandBufferBase<Type> command_buffer{};

    command_buffer.begin();

    func(command_buffer);

    command_buffer.end();

    command_buffer.submit();

    vkQueueWaitIdle(get_queue()->handle());
}

template <CommandBufferType Type>
std::shared_ptr<VulkanQueue> VulkanCommandBufferBase<Type>::get_queue()
{
    switch (Type)
    {
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

void VulkanRenderCommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    const auto& native_pipeline = std::dynamic_pointer_cast<VulkanGraphicsPipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, native_pipeline->handle());

    m_currently_bound_shader = native_pipeline->get_shader();
}

void VulkanRenderCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    const auto& native_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, native_pipeline->handle());

    m_currently_bound_shader = native_pipeline->get_shader();
}

void VulkanRenderCommandBuffer::begin_render_pass(const RenderPass& render_pass) const
{
    const VulkanRenderPass& native_render_pass = dynamic_cast<const VulkanRenderPass&>(render_pass);
    const VulkanFramebuffer& native_framebuffer =
        dynamic_cast<const VulkanFramebuffer&>(*render_pass.get_framebuffer());

    begin_render_pass(native_render_pass, native_framebuffer);
}

void VulkanRenderCommandBuffer::begin_render_pass(const VulkanRenderPass& render_pass,
                                                  const VulkanFramebuffer& framebuffer) const
{
    render_pass.begin(m_command_buffer, framebuffer.handle());

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(framebuffer.get_width());
    viewport.height = static_cast<float>(framebuffer.get_height());
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {framebuffer.get_width(), framebuffer.get_height()};

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void VulkanRenderCommandBuffer::end_render_pass(const RenderPass& render_pass) const
{
    const VulkanRenderPass& native_render_pass = dynamic_cast<const VulkanRenderPass&>(render_pass);
    native_render_pass.end(m_command_buffer);
}

void VulkanRenderCommandBuffer::draw(const VertexBuffer& vertex) const
{
    const auto& native_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(vertex.get_resource());

    const std::array<VkBuffer, 1> vertex_buffers = {native_buffer->handle()};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(m_command_buffer, 0, vertex_buffers.size(), vertex_buffers.data(), offsets);

    vkCmdDraw(m_command_buffer, vertex.get_count(), 1, 0, 0);
}

void VulkanRenderCommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    {
        const auto& vertex_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(vertex.get_resource());

        const std::array<VkBuffer, 1> vertex_buffers = {vertex_buffer->handle()};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(m_command_buffer, 0, vertex_buffers.size(), vertex_buffers.data(), offsets);
    }

    {
        const auto& index_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(index.get_resource());
        vkCmdBindIndexBuffer(m_command_buffer, index_buffer->handle(), 0, VK_INDEX_TYPE_UINT32);
    }

    vkCmdDrawIndexed(m_command_buffer, index.get_count(), 1, 0, 0, 0);
}

void VulkanRenderCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

//
// VulkanComputeCommandBuffer
//

void VulkanComputeCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    const auto& native_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, native_pipeline->handle());

    m_currently_bound_shader = native_pipeline->get_shader();
}

void VulkanComputeCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

//
// Specializations
//

template class VulkanCommandBufferBase<CommandBufferType::Graphics>;
template class VulkanCommandBufferBase<CommandBufferType::Compute>;
template class VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
