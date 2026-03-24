#include "vulkan_command_buffer.h"

#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "vulkan_acceleration_structure.h"
#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_descriptors.h"
#include "vulkan_image_resource.h"
#include "vulkan_pipeline.h"
#include "vulkan_queue.h"
#include "vulkan_resource_view.h"
#include "vulkan_shader.h"
#include "vulkan_synchronization.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

VulkanCommandBuffer::VulkanCommandBuffer(CommandBufferType type) : m_type(type)
{
    m_command_buffer = VulkanContext.device->allocate_command_buffer(m_type);
    MIZU_ASSERT(m_command_buffer != VK_NULL_HANDLE, "Error allocating command buffers");
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    VulkanContext.device->free_command_buffer(m_command_buffer, m_type);
}

void VulkanCommandBuffer::begin()
{
    VK_CHECK(vkResetCommandBuffer(m_command_buffer, 0));

    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VK_CHECK(vkBeginCommandBuffer(m_command_buffer, &info));
}

void VulkanCommandBuffer::end()
{
    VK_CHECK(vkEndCommandBuffer(m_command_buffer));
}

void VulkanCommandBuffer::submit(const CommandBufferSubmitInfo& info) const
{
    inplace_vector<VkSemaphore, CommandBufferSubmitInfo::MAX_SEMAPHORES> native_wait_semaphores;
    inplace_vector<VkPipelineStageFlags, CommandBufferSubmitInfo::MAX_SEMAPHORES> native_wait_dst_stage_masks;

    for (const std::shared_ptr<Semaphore>& wait_semaphore : info.wait_semaphores)
    {
        const VulkanSemaphore& vk_wait_semaphore = static_cast<const VulkanSemaphore&>(*wait_semaphore);
        native_wait_semaphores.push_back(vk_wait_semaphore.handle());

        VkPipelineStageFlags stage_flags = VK_PIPELINE_STAGE_NONE;
        switch (m_type)
        {
        case CommandBufferType::Graphics:
            stage_flags = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
            break;
        case CommandBufferType::Compute:
            stage_flags = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
            break;
        case CommandBufferType::Transfer:
            stage_flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        }

        native_wait_dst_stage_masks.push_back(stage_flags);
    }

    std::vector<VkSemaphore> signal_semaphores;

    for (const std::shared_ptr<Semaphore>& signal_semaphore : info.signal_semaphores)
    {
        const VulkanSemaphore& vk_signal_semaphore = static_cast<const VulkanSemaphore&>(*signal_semaphore);
        signal_semaphores.push_back(vk_signal_semaphore.handle());
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(native_wait_semaphores.size());
    submit_info.pWaitSemaphores = native_wait_semaphores.data();
    submit_info.pWaitDstStageMask = native_wait_dst_stage_masks.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    const VkFence signal_fence = info.signal_fence != nullptr
                                     ? std::static_pointer_cast<VulkanFence>(info.signal_fence)->handle()
                                     : VK_NULL_HANDLE;

    get_queue()->submit(submit_info, signal_fence);
}

void VulkanCommandBuffer::bind_descriptor_set(std::shared_ptr<DescriptorSet> descriptor_set, uint32_t set)
{
    const VulkanDescriptorSet& native_descriptor_set = static_cast<const VulkanDescriptorSet&>(*descriptor_set);

    VkDescriptorSet native_set = native_descriptor_set.handle();
    vkCmdBindDescriptorSets(
        m_command_buffer,
        VulkanPipeline::get_vulkan_pipeline_bind_point(m_bound_pipeline->get_pipeline_type()),
        m_bound_pipeline->get_pipeline_layout(),
        set,
        1,
        &native_set,
        0,
        nullptr);
}

void VulkanCommandBuffer::push_constant(uint32_t size, const void* data) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't push constant when no pipeline has been bound");

    const std::optional<PushConstantItem> constant_info_opt = m_bound_pipeline->get_push_constant_info();
    MIZU_ASSERT(constant_info_opt.has_value(), "Bound pipeline does not have a push constant");

    const PushConstantItem& constant_info = *constant_info_opt;
    // MIZU_ASSERT(
    //     constant_info.size == size,
    //     "Size of push constant does not match expected (size = {}, expected = {})",
    //     size,
    //     constant_info.size);

    const VkShaderStageFlags vk_stage_flags = VulkanShader::get_vulkan_shader_stage_bits(constant_info.stage);
    vkCmdPushConstants(m_command_buffer, m_bound_pipeline->get_pipeline_layout(), vk_stage_flags, 0, size, data);
}

void VulkanCommandBuffer::begin_render_pass(const RenderPassInfo& info)
{
    MIZU_ASSERT(!m_render_pass_active, "Can't bind RenderPass because a RenderPass is already active");
    MIZU_ASSERT(m_type == CommandBufferType::Graphics, "Can't begin render pass non Graphics Command Buffer");

    VkExtent2D extent{};
    extent.width = info.extent.x;
    extent.height = info.extent.y;

    VkOffset2D offset{};
    offset.x = info.offset.x;
    offset.y = info.offset.y;

    inplace_vector<VkRenderingAttachmentInfo, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments;
    VkRenderingAttachmentInfo depth_stencil_attachment;

    for (const FramebufferAttachment& attachment : info.color_attachments)
    {
        const ImageResourceView& rtv = attachment.rtv;
        MIZU_ASSERT(rtv.image != nullptr, "Invalid image in rtv");

        VulkanImageResource& native_rtv_image = static_cast<VulkanImageResource&>(*rtv.image);

        const VulkanImageResourceView internal_rtv = native_rtv_image.as_rtv(rtv.desc);
        MIZU_ASSERT(!is_depth_format(internal_rtv.format), "Can't use a rtv with a depth format as a color attachment");

        VkRenderingAttachmentInfo& attachment_info = color_attachments.emplace_back();
        attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        attachment_info.pNext = nullptr;
        attachment_info.imageView = internal_rtv.handle;
        attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        attachment_info.loadOp = get_vulkan_load_operation(attachment.load_operation);
        attachment_info.storeOp = get_vulkan_store_operation(attachment.store_operation);
        attachment_info.clearValue = VkClearValue{
            .color = {{
                attachment.clear_value.r,
                attachment.clear_value.g,
                attachment.clear_value.b,
                attachment.clear_value.a,
            }},
        };
    }

    if (info.depth_stencil_attachment.has_value())
    {
        const FramebufferAttachment& attachment = *info.depth_stencil_attachment;

        const ImageResourceView& rtv = attachment.rtv;
        MIZU_ASSERT(rtv.image != nullptr, "Invalid image in rtv");

        VulkanImageResource& native_rtv_image = static_cast<VulkanImageResource&>(*rtv.image);

        const VulkanImageResourceView internal_rtv = native_rtv_image.as_rtv(rtv.desc);
        MIZU_ASSERT(
            is_depth_format(internal_rtv.format), "Can't use a rtv with a non depth format as a depth attachment");

        depth_stencil_attachment = VkRenderingAttachmentInfo{};
        depth_stencil_attachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_stencil_attachment.pNext = nullptr;
        depth_stencil_attachment.imageView = internal_rtv.handle;
        depth_stencil_attachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_stencil_attachment.loadOp = get_vulkan_load_operation(attachment.load_operation);
        depth_stencil_attachment.storeOp = get_vulkan_store_operation(attachment.store_operation);
        depth_stencil_attachment.clearValue = VkClearValue{
            .depthStencil = {.depth = attachment.clear_value.r, .stencil = 0},
        };
    }

    VkRenderingInfo rendering_info{};
    rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering_info.pNext = nullptr;
    rendering_info.flags = 0;
    rendering_info.renderArea = VkRect2D{.offset = offset, .extent = extent};
    rendering_info.layerCount = 1;
    rendering_info.viewMask = 0;
    rendering_info.colorAttachmentCount = static_cast<uint32_t>(color_attachments.size());
    rendering_info.pColorAttachments = color_attachments.data();
    rendering_info.pDepthAttachment = info.depth_stencil_attachment.has_value() ? &depth_stencil_attachment : nullptr;
    rendering_info.pStencilAttachment = nullptr;

    vkCmdBeginRendering(m_command_buffer, &rendering_info);

    VkViewport viewport{};
    viewport.x = static_cast<float>(offset.x);
    viewport.y = static_cast<float>(offset.y);
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = info.min_depth;
    viewport.maxDepth = info.max_depth;

    VkRect2D scissor{};
    scissor.offset = offset;
    scissor.extent = extent;

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);

    m_render_pass_active = true;
}

void VulkanCommandBuffer::end_render_pass()
{
    MIZU_ASSERT(m_render_pass_active, "Can't end RenderPass because no RenderPass is active");

    vkCmdEndRendering(m_command_buffer);

    m_render_pass_active = false;
}

void VulkanCommandBuffer::bind_pipeline(std::shared_ptr<Pipeline> pipeline)
{
#if MIZU_DEBUG
    if (m_render_pass_active)
    {
        MIZU_ASSERT(
            pipeline->get_pipeline_type() == PipelineType::Graphics,
            "Can't bind non graphics pipeline if a render pass is active");
    }
    else
    {
        MIZU_ASSERT(
            pipeline->get_pipeline_type() != PipelineType::Graphics,
            "Can't bind graphics pipeline because no render pass is active");
    }
#endif

    m_bound_pipeline = std::static_pointer_cast<VulkanPipeline>(pipeline);

    const VkPipeline handle = m_bound_pipeline->handle();
    const VkPipelineBindPoint bind_point =
        VulkanPipeline::get_vulkan_pipeline_bind_point(m_bound_pipeline->get_pipeline_type());

    vkCmdBindPipeline(m_command_buffer, bind_point, handle);
}

void VulkanCommandBuffer::draw(const BufferResource& vertex) const
{
    draw_instanced(vertex, 1);
}

void VulkanCommandBuffer::draw_indexed(const BufferResource& vertex, const BufferResource& index) const
{
    draw_indexed_instanced(vertex, index, 1);
}

void VulkanCommandBuffer::draw_instanced(const BufferResource& vertex, uint32_t instance_count) const
{
    MIZU_ASSERT(m_render_pass_active, "Can't draw_instanced because no RenderPass is active");
    MIZU_ASSERT(
        m_bound_pipeline != nullptr && m_bound_pipeline->get_pipeline_type() == PipelineType::Graphics,
        "Can't draw_indexed_instance because no graphics pipeline has been bound");

    const VulkanBufferResource& native_buffer = static_cast<const VulkanBufferResource&>(vertex);

    const std::array<VkBuffer, 1> vertex_buffers = {native_buffer.handle()};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(
        m_command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets);

    const uint32_t vertex_count = static_cast<uint32_t>(vertex.get_size() / vertex.get_stride());
    vkCmdDraw(m_command_buffer, vertex_count, instance_count, 0, 0);
}

void VulkanCommandBuffer::draw_indexed_instanced(
    const BufferResource& vertex,
    const BufferResource& index,
    uint32_t instance_count) const
{
    MIZU_ASSERT(m_render_pass_active, "Can't draw_indexed_instance because no RenderPass is active");
    MIZU_ASSERT(
        m_bound_pipeline != nullptr && m_bound_pipeline->get_pipeline_type() == PipelineType::Graphics,
        "Can't draw_indexed_instance because no graphics pipeline has been bound");

    {
        const VulkanBufferResource& vertex_buffer = static_cast<const VulkanBufferResource&>(vertex);

        const std::array<VkBuffer, 1> vertex_buffers = {vertex_buffer.handle()};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(
            m_command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets);
    }

    {
        const VulkanBufferResource& index_buffer = static_cast<const VulkanBufferResource&>(index);
        vkCmdBindIndexBuffer(m_command_buffer, index_buffer.handle(), 0, VK_INDEX_TYPE_UINT32);
    }

    const uint32_t index_count = static_cast<uint32_t>(index.get_size() / sizeof(uint32_t));
    vkCmdDrawIndexed(m_command_buffer, index_count, instance_count, 0, 0, 0);
}

void VulkanCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(
        m_bound_pipeline != nullptr && m_bound_pipeline->get_pipeline_type() == PipelineType::Compute,
        "Can't draw_indexed_instance because no compute pipeline has been bound");

    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

void VulkanCommandBuffer::trace_rays(glm::uvec3 dimensions) const
{
    MIZU_ASSERT(
        m_bound_pipeline != nullptr && m_bound_pipeline->get_pipeline_type() == PipelineType::RayTracing,
        "Can't draw_indexed_instance because no ray tracing pipeline has been bound");

    const VkStridedDeviceAddressRegionKHR& raygen_region = m_bound_pipeline->get_ray_generation_region();
    const VkStridedDeviceAddressRegionKHR& miss_region = m_bound_pipeline->get_miss_region();
    const VkStridedDeviceAddressRegionKHR& hit_region = m_bound_pipeline->get_hit_region();
    const VkStridedDeviceAddressRegionKHR& call_region = m_bound_pipeline->get_call_region();

    vkCmdTraceRaysKHR(
        m_command_buffer,
        &raygen_region,
        &miss_region,
        &hit_region,
        &call_region,
        dimensions.x,
        dimensions.y,
        dimensions.z);
}

static uint32_t get_vulkan_transition_queue_family_index(const std::optional<CommandBufferType>& type)
{
    if (!type.has_value())
        return VK_QUEUE_FAMILY_IGNORED;

    return VulkanContext.device->get_queue(*type)->family();
}

void VulkanCommandBuffer::transition_resource(const BufferResource& buffer, const BufferTransitionInfo& info) const
{
    if (info.old_state == info.new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    // In vulkan, only release and acquire transfer modes require a pipeline barrier, normal resource state transitions
    // don't require a pipeline barrier
    if (info.transition_mode == ResourceTransitionMode::Normal)
        return;

    const VulkanBufferResource& native_buffer = static_cast<const VulkanBufferResource&>(buffer);

    const uint32_t src_queue_family = get_vulkan_transition_queue_family_index(info.src_queue_family);
    const uint32_t dst_queue_family = get_vulkan_transition_queue_family_index(info.dst_queue_family);

    if ((src_queue_family != VK_QUEUE_FAMILY_IGNORED || dst_queue_family != VK_QUEUE_FAMILY_IGNORED)
        && native_buffer.get_sharing_mode() == ResourceSharingMode::Concurrent)
    {
        MIZU_LOG_WARNING(
            "Specifying source of destination queue family when resource has ResourceSharingMode::Concurrent");
    }

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.buffer = native_buffer.handle();
    barrier.offset = info.offset;
    barrier.size = info.size;

    const auto get_vulkan_access_mask = [](BufferResourceState state) -> VkAccessFlags {
        switch (state)
        {
        case BufferResourceState::Undefined:
            return 0;
        case BufferResourceState::ShaderReadOnly:
            return VK_ACCESS_SHADER_READ_BIT;
        case BufferResourceState::UnorderedAccess:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case BufferResourceState::TransferSrc:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case BufferResourceState::TransferDst:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case BufferResourceState::AccelStructScratch:
        case BufferResourceState::AccelStructBuildInput:
            MIZU_UNREACHABLE("Not implemented");
            return VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM;
        }
    };

    const auto get_vulkan_pipeline_stage_flags = [&](BufferResourceState state) -> VkPipelineStageFlags {
        switch (state)
        {
        case BufferResourceState::Undefined:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case BufferResourceState::ShaderReadOnly:
            if (m_type == CommandBufferType::Graphics)
            {
                return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                       | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            else
            {
                return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
        case BufferResourceState::UnorderedAccess:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case BufferResourceState::TransferSrc:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case BufferResourceState::TransferDst:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case BufferResourceState::AccelStructScratch:
        case BufferResourceState::AccelStructBuildInput:
            MIZU_UNREACHABLE("Not implemented");
            return VK_PIPELINE_STAGE_FLAG_BITS_MAX_ENUM;
        }
    };

    barrier.srcAccessMask = get_vulkan_access_mask(info.old_state);
    barrier.dstAccessMask = get_vulkan_access_mask(info.new_state);

    VkPipelineStageFlags src_stage = get_vulkan_pipeline_stage_flags(info.old_state);
    VkPipelineStageFlags dst_stage = get_vulkan_pipeline_stage_flags(info.new_state);

    if (info.transition_mode == ResourceTransitionMode::Release)
    {
        barrier.dstAccessMask = 0;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else if (info.transition_mode == ResourceTransitionMode::Acquire)
    {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanCommandBuffer::transition_resource(const ImageResource& image, const ImageTransitionInfo& info) const
{
    if (info.old_state == info.new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(image);

    const VkImageLayout old_layout = get_vulkan_image_resource_state(info.old_state);
    const VkImageLayout new_layout = get_vulkan_image_resource_state(info.new_state);

    const uint32_t src_queue_family = get_vulkan_transition_queue_family_index(info.src_queue_family);
    const uint32_t dst_queue_family = get_vulkan_transition_queue_family_index(info.dst_queue_family);

    if ((src_queue_family != VK_QUEUE_FAMILY_IGNORED || dst_queue_family != VK_QUEUE_FAMILY_IGNORED)
        && native_image.get_sharing_mode() == ResourceSharingMode::Concurrent)
    {
        MIZU_LOG_WARNING(
            "Specifying source of destination queue family when resource has ResourceSharingMode::Concurrent");
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.image = native_image.handle();
    barrier.subresourceRange.aspectMask =
        is_depth_format(image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = info.view_desc.mip_base;
    barrier.subresourceRange.levelCount = info.view_desc.mip_count;
    barrier.subresourceRange.baseArrayLayer = info.view_desc.layer_base;
    barrier.subresourceRange.layerCount = info.view_desc.layer_count;

    const auto get_vulkan_access_mask = [](ImageResourceState state) -> VkAccessFlags {
        switch (state)
        {
        case ImageResourceState::Undefined:
            return 0;
        case ImageResourceState::ShaderReadOnly:
            return VK_ACCESS_SHADER_READ_BIT;
        case ImageResourceState::UnorderedAccess:
            return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        case ImageResourceState::TransferSrc:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case ImageResourceState::TransferDst:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case ImageResourceState::ColorAttachment:
            return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case ImageResourceState::DepthStencilAttachment:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case ImageResourceState::Present:
            return VK_ACCESS_MEMORY_READ_BIT;
        }
    };

    const auto get_vulkan_pipeline_stage_flags = [&](ImageResourceState state) -> VkPipelineStageFlags {
        switch (state)
        {
        case ImageResourceState::Undefined:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case ImageResourceState::ShaderReadOnly:
            if (m_type == CommandBufferType::Graphics)
            {
                return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                       | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
            else
            {
                return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            }
        case ImageResourceState::UnorderedAccess:
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case ImageResourceState::TransferSrc:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case ImageResourceState::TransferDst:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case ImageResourceState::ColorAttachment:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case ImageResourceState::DepthStencilAttachment:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case ImageResourceState::Present:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }
    };

    barrier.srcAccessMask = get_vulkan_access_mask(info.old_state);
    barrier.dstAccessMask = get_vulkan_access_mask(info.new_state);

    VkPipelineStageFlags src_stage = get_vulkan_pipeline_stage_flags(info.old_state);
    VkPipelineStageFlags dst_stage = get_vulkan_pipeline_stage_flags(info.new_state);

    if (info.transition_mode == ResourceTransitionMode::Release)
    {
        barrier.dstAccessMask = 0;
        dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else if (info.transition_mode == ResourceTransitionMode::Acquire)
    {
        barrier.srcAccessMask = 0;
        src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }

    vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandBuffer::transition_resource(
    const AccelerationStructure& accel_struct,
    const AccelerationStructureTransitionInfo& info) const
{
    if (info.old_state == info.new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const VulkanAccelerationStructure& native_as = static_cast<const VulkanAccelerationStructure&>(accel_struct);

    const auto get_vulkan_access_mask = [](AccelerationStructureResourceState state) -> VkAccessFlags {
        switch (state)
        {
        case AccelerationStructureResourceState::Undefined:
            return 0;
        case AccelerationStructureResourceState::AccelStructRead:
            return VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
        case AccelerationStructureResourceState::AccelStructWrite:
            return VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
        default:
            MIZU_ASSERT(false, "Invalid acceleration structure resource state");
            return 0;
        }
    };

    const auto get_vulkan_pipeline_stage_flags = [](AccelerationStructureResourceState state) -> VkPipelineStageFlags {
        switch (state)
        {
        case AccelerationStructureResourceState::Undefined:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case AccelerationStructureResourceState::AccelStructRead:
            return VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR
                   | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        case AccelerationStructureResourceState::AccelStructWrite:
            return VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
        default:
            MIZU_ASSERT(false, "Invalid acceleration structure resource state");
            return 0;
        }
    };

    const uint32_t src_queue_family = get_vulkan_transition_queue_family_index(info.src_queue_family);
    const uint32_t dst_queue_family = get_vulkan_transition_queue_family_index(info.dst_queue_family);

    VkBufferMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = src_queue_family;
    barrier.dstQueueFamilyIndex = dst_queue_family;
    barrier.buffer = native_as.get_as_buffer()->handle();
    barrier.offset = 0;
    barrier.size = VK_WHOLE_SIZE;
    barrier.srcAccessMask = get_vulkan_access_mask(info.old_state);
    barrier.dstAccessMask = get_vulkan_access_mask(info.new_state);

    if (info.transition_mode == ResourceTransitionMode::Release)
        barrier.dstAccessMask = 0;
    else if (info.transition_mode == ResourceTransitionMode::Acquire)
        barrier.srcAccessMask = 0;

    const VkPipelineStageFlags src_stage = get_vulkan_pipeline_stage_flags(info.old_state);
    const VkPipelineStageFlags dst_stage = get_vulkan_pipeline_stage_flags(info.new_state);

    vkCmdPipelineBarrier(m_command_buffer, src_stage, dst_stage, 0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanCommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    MIZU_ASSERT(
        source.get_size() == dest.get_size(),
        "Size of buffers do not match ({} != {})",
        source.get_size(),
        dest.get_size());

    const VulkanBufferResource& native_source = static_cast<const VulkanBufferResource&>(source);
    const VulkanBufferResource& native_dest = static_cast<const VulkanBufferResource&>(dest);

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = source.get_size();

    vkCmdCopyBuffer(m_command_buffer, native_source.handle(), native_dest.handle(), 1, &copy);
}

void VulkanCommandBuffer::copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const
{
    const VulkanImageResource& native_image = static_cast<const VulkanImageResource&>(image);
    const VulkanBufferResource& native_buffer = static_cast<const VulkanBufferResource&>(buffer);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask =
        is_depth_format(native_image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = native_image.get_num_layers();
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {native_image.get_width(), native_image.get_height(), native_image.get_depth()};

    vkCmdCopyBufferToImage(
        m_command_buffer,
        native_buffer.handle(),
        native_image.handle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
}

void VulkanCommandBuffer::build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const
{
    MIZU_ASSERT(blas.get_type() == AccelerationStructureType::BottomLevel, "Acceleration structure is not BLAS");

    const VulkanAccelerationStructure& native_blas = static_cast<const VulkanAccelerationStructure&>(blas);
    const VulkanBufferResource& native_scratch_buffer = static_cast<const VulkanBufferResource&>(scratch_buffer);

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info = native_blas.get_build_geometry_info();
    VkAccelerationStructureBuildRangeInfoKHR build_range_info = native_blas.get_build_range_info();

    build_geometry_info.scratchData.deviceAddress = get_device_address(native_scratch_buffer.handle());

    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &build_range_info;
    vkCmdBuildAccelerationStructuresKHR(m_command_buffer, 1, &build_geometry_info, &build_range);
}

static void build_tlas_internal(
    VkCommandBuffer command,
    const VulkanAccelerationStructure& tlas,
    VkAccelerationStructureBuildGeometryInfoKHR build_geometry,
    VkAccelerationStructureBuildRangeInfoKHR range_info,
    std::span<AccelerationStructureInstanceData> instances,
    const VulkanBufferResource& scratch_buffer)
{
    MIZU_ASSERT(tlas.get_type() == AccelerationStructureType::TopLevel, "Acceleration structure is not TLAS");
    MIZU_ASSERT(
        instances.size() == range_info.primitiveCount,
        "Number of BLAS instances does not match requested number of instances");

    std::vector<VkAccelerationStructureInstanceKHR> instances_data;

    for (uint32_t i = 0; i < instances.size(); ++i)
    {
        const AccelerationStructureInstanceData& data = instances[i];
        MIZU_ASSERT(data.blas != nullptr, "Blas at index {} is nullptr", i);

        VkTransformMatrixKHR vk_transform{};
        for (int32_t r = 0; r < 3; ++r)
        {
            for (int32_t c = 0; c < 4; ++c)
            {
                vk_transform.matrix[r][c] = data.transform[c][r];
            }
        }

        const VulkanAccelerationStructure& native_blas = static_cast<const VulkanAccelerationStructure&>(*data.blas);
        const VkDeviceAddress blas_address = get_device_address(native_blas.handle());

        VkAccelerationStructureInstanceKHR instance_data{};
        instance_data.transform = vk_transform;
        instance_data.instanceCustomIndex = i;
        instance_data.mask = 0xff;
        instance_data.instanceShaderBindingTableRecordOffset = 0;
        instance_data.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        instance_data.accelerationStructureReference = blas_address;

        instances_data.push_back(instance_data);
    }

    const uint8_t* instances_data_ptr = reinterpret_cast<const uint8_t*>(instances_data.data());

    const VulkanBufferResource& instances_buffer = tlas.get_instances_buffer();
    instances_buffer.set_data(instances_data_ptr);

    build_geometry.scratchData.deviceAddress = get_device_address(scratch_buffer.handle());

    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &range_info;
    vkCmdBuildAccelerationStructuresKHR(command, 1, &build_geometry, &build_range);
}

void VulkanCommandBuffer::build_tlas(
    const AccelerationStructure& tlas,
    std::span<AccelerationStructureInstanceData> instances,
    const BufferResource& scratch_buffer) const
{
    const VulkanAccelerationStructure& native_tlas = static_cast<const VulkanAccelerationStructure&>(tlas);
    const VulkanBufferResource& native_scratch_buffer = static_cast<const VulkanBufferResource&>(scratch_buffer);

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info = native_tlas.get_build_geometry_info();
    VkAccelerationStructureBuildRangeInfoKHR build_range_info = native_tlas.get_build_range_info();

    build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    build_tlas_internal(
        m_command_buffer, native_tlas, build_geometry_info, build_range_info, instances, native_scratch_buffer);
}

void VulkanCommandBuffer::update_tlas(
    const AccelerationStructure& tlas,
    std::span<AccelerationStructureInstanceData> instances,
    const BufferResource& scratch_buffer) const
{
    const VulkanAccelerationStructure& native_tlas = static_cast<const VulkanAccelerationStructure&>(tlas);
    const VulkanBufferResource& native_scratch_buffer = static_cast<const VulkanBufferResource&>(scratch_buffer);

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info = native_tlas.get_build_geometry_info();
    VkAccelerationStructureBuildRangeInfoKHR build_range_info = native_tlas.get_build_range_info();

    build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    build_geometry_info.srcAccelerationStructure = native_tlas.handle();

    build_tlas_internal(
        m_command_buffer, native_tlas, build_geometry_info, build_range_info, instances, native_scratch_buffer);
}

void VulkanCommandBuffer::begin_gpu_marker(std::string_view label) const
{
    VK_DEBUG_BEGIN_GPU_MARKER(m_command_buffer, label);
}

void VulkanCommandBuffer::end_gpu_marker() const
{
    VK_DEBUG_END_GPU_MARKER(m_command_buffer);
}

std::shared_ptr<VulkanQueue> VulkanCommandBuffer::get_queue() const
{
    return VulkanContext.device->get_queue(m_type);
}

} // namespace Mizu::Vulkan
