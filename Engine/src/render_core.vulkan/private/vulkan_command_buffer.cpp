#include "vulkan_command_buffer.h"

#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "vulkan_acceleration_structure.h"
#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_descriptors2.h"
#include "vulkan_framebuffer.h"
#include "vulkan_image_resource.h"
#include "vulkan_pipeline.h"
#include "vulkan_queue.h"
#include "vulkan_resource_group.h"
#include "vulkan_resource_view.h"
#include "vulkan_shader.h"
#include "vulkan_synchronization.h"

namespace Mizu::Vulkan
{

VulkanCommandBuffer::VulkanCommandBuffer(CommandBufferType type) : m_type(type)
{
    // TODO: 10 is an arbitrary number, should query max number of descriptor sets from device capabilities
    m_bound_resource_groups.resize(10);

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
    clear_bound_resource_groups();

    VK_CHECK(vkEndCommandBuffer(m_command_buffer));
}

void VulkanCommandBuffer::submit(const CommandBufferSubmitInfo& info) const
{
    std::vector<VkSemaphore> wait_semaphores;
    std::vector<VkPipelineStageFlags> wait_dst_stage_masks;

    for (const std::shared_ptr<Semaphore>& wait_semaphore : info.wait_semaphores)
    {
        const VulkanSemaphore& vk_wait_semaphore = dynamic_cast<const VulkanSemaphore&>(*wait_semaphore);
        wait_semaphores.push_back(vk_wait_semaphore.handle());

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

        wait_dst_stage_masks.push_back(stage_flags);
    }

    std::vector<VkSemaphore> signal_semaphores;

    for (const std::shared_ptr<Semaphore>& signal_semaphore : info.signal_semaphores)
    {
        const VulkanSemaphore& vk_signal_semaphore = dynamic_cast<const VulkanSemaphore&>(*signal_semaphore);
        signal_semaphores.push_back(vk_signal_semaphore.handle());
    }

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_dst_stage_masks.data();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffer;
    submit_info.signalSemaphoreCount = static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    const VkFence signal_fence = info.signal_fence != nullptr
                                     ? std::dynamic_pointer_cast<VulkanFence>(info.signal_fence)->handle()
                                     : VK_NULL_HANDLE;

    get_queue()->submit(submit_info, signal_fence);
}

void VulkanCommandBuffer::bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set)
{
    MIZU_ASSERT(
        set < m_bound_resource_groups.size(),
        "Set is bigger than max number of resource groups ({} >= {})",
        set,
        m_bound_resource_groups.size());
    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't bind resource group when no pipeline has been bound");

    ResourceGroupInfo& info = m_bound_resource_groups[set];

    if (info.has_value() && info.resource_group->get_hash() == resource_group->get_hash())
    {
        // Is the same resource, no need to bind again
        return;
    }

    const auto native_resource_group = std::dynamic_pointer_cast<VulkanResourceGroup>(resource_group);
    // MIZU_ASSERT(
    //     native_resource_group->get_descriptor_set_layout() == m_bound_pipeline->get_descriptor_set_layout(set),
    //     "Can't bind resource group because the resource group layout does not match the layout in the pipeline");

    info.resource_group = native_resource_group;
    info.set = set;

    const VkDescriptorSet& descriptor_set = native_resource_group->get_descriptor_set();
    vkCmdBindDescriptorSets(
        m_command_buffer,
        VulkanPipeline::get_vulkan_pipeline_bind_point(m_bound_pipeline->get_pipeline_type()),
        m_bound_pipeline->get_pipeline_layout(),
        set,
        1,
        &descriptor_set,
        0,
        nullptr);
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

void VulkanCommandBuffer::begin_render_pass(std::shared_ptr<Framebuffer> framebuffer)
{
    MIZU_ASSERT(m_active_render_pass == nullptr, "Can't bind RenderPass because a RenderPass is already active");
    MIZU_ASSERT(m_type == CommandBufferType::Graphics, "Can't begin render pass non Graphics Command Buffer");

    clear_bound_resource_groups();

    m_active_render_pass = std::static_pointer_cast<VulkanFramebuffer>(framebuffer);

    const uint32_t width = framebuffer->get_width();
    const uint32_t height = framebuffer->get_height();

    const std::span<const VkClearValue> clear_values = m_active_render_pass->get_clear_values();

    VkRenderPassBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    begin_info.renderPass = m_active_render_pass->get_render_pass();
    begin_info.framebuffer = m_active_render_pass->handle();
    begin_info.renderArea.offset = {0, 0};
    begin_info.renderArea.extent = {
        width,
        height,
    };
    begin_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    begin_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(m_command_buffer, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(width);
    viewport.height = static_cast<float>(height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {width, height};

    vkCmdSetViewport(m_command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(m_command_buffer, 0, 1, &scissor);
}

void VulkanCommandBuffer::VulkanCommandBuffer::end_render_pass()
{
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't end RenderPass because no RenderPass is active");

    vkCmdEndRenderPass(m_command_buffer);
    m_active_render_pass = nullptr;

    clear_bound_resource_groups();
}

void VulkanCommandBuffer::bind_pipeline(std::shared_ptr<Pipeline> pipeline)
{
#if MIZU_DEBUG
    if (m_active_render_pass != nullptr)
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
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't draw_instanced because no RenderPass is active");
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
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't draw_indexed_instance because no RenderPass is active");
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

void VulkanCommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state) const
{
    const ImageResourceViewDescription range = {
        .mip_base = 0,
        .mip_count = image.get_num_mips(),
        .layer_base = 0,
        .layer_count = image.get_num_layers(),
    };

    transition_resource(image, old_state, new_state, range);
}

void VulkanCommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state,
    ImageResourceViewDescription range) const
{
    struct TransitionInfo
    {
        VkPipelineStageFlags src_stage, dst_stage;
        VkAccessFlags src_access_mask, dst_access_mask;
    };

    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const VulkanImageResource& native_image = dynamic_cast<const VulkanImageResource&>(image);

    const VkImageLayout old_layout = VulkanImageResource::get_vulkan_image_resource_state(old_state);
    const VkImageLayout new_layout = VulkanImageResource::get_vulkan_image_resource_state(new_state);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = native_image.handle();
    barrier.subresourceRange.aspectMask =
        is_depth_format(image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = range.mip_base;
    barrier.subresourceRange.levelCount = range.mip_count;
    barrier.subresourceRange.baseArrayLayer = range.layer_base;
    barrier.subresourceRange.layerCount = range.layer_count;

#define DEFINE_TRANSITION(oldl, newl, src_mask, dst_mask, src_stage, dst_stage) \
    {                                                                           \
        {ImageResourceState::oldl, ImageResourceState::newl}, TransitionInfo    \
        {                                                                       \
            src_stage, dst_stage, src_mask, dst_mask                            \
        }                                                                       \
    }

    // NOTE: At the moment only specifying "expected transitions"
    static std::map<std::pair<ImageResourceState, ImageResourceState>, TransitionInfo> s_transition_info{
        // Undefined
        DEFINE_TRANSITION(
            Undefined,
            UnorderedAccess,
            0,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),

        DEFINE_TRANSITION(
            Undefined,
            TransferDst,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT),

        DEFINE_TRANSITION(
            Undefined,
            ColorAttachment,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),

        DEFINE_TRANSITION(
            Undefined,
            DepthStencilAttachment,
            0,
            VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        // UnorderedAccess
        DEFINE_TRANSITION(
            UnorderedAccess,
            ShaderReadOnly,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        DEFINE_TRANSITION(
            ShaderReadOnly,
            DepthStencilAttachment,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT),

        DEFINE_TRANSITION(
            UnorderedAccess,
            Present,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            0,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),

        // TransferDst
        DEFINE_TRANSITION(
            TransferDst,
            ShaderReadOnly,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        // ShaderReadOnly
        DEFINE_TRANSITION(
            ShaderReadOnly,
            UnorderedAccess,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),

        DEFINE_TRANSITION(
            ShaderReadOnly,
            Present,
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),

        // ColorAttachment
        DEFINE_TRANSITION(
            ColorAttachment,
            ShaderReadOnly,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        DEFINE_TRANSITION(
            ColorAttachment,
            Present,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_MEMORY_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),

        // DepthStencilAttachment
        DEFINE_TRANSITION(
            DepthStencilAttachment,
            ShaderReadOnly,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),
    };

#undef DEFINE_TRANSITION

    const auto it = s_transition_info.find({old_state, new_state});
    if (it == s_transition_info.end())
    {
        MIZU_UNREACHABLE(
            "Image layout transition not defined: {} -> {} for texture: {}",
            image_resource_state_to_string(old_state),
            image_resource_state_to_string(new_state),
            native_image.get_name());
        return;
    }

    const TransitionInfo& info = it->second;

    barrier.srcAccessMask = info.src_access_mask;
    barrier.dstAccessMask = info.dst_access_mask;

    vkCmdPipelineBarrier(m_command_buffer, info.src_stage, info.dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandBuffer::transition_resource(
    [[maybe_unused]] const BufferResource& buffer,
    [[maybe_unused]] BufferResourceState old_state,
    [[maybe_unused]] BufferResourceState new_state) const
{
    // No need to transition buffer resources in Vulkan
}

void VulkanCommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    MIZU_ASSERT(
        source.get_size() == dest.get_size(),
        "Size of buffers do not match ({} != {})",
        source.get_size(),
        dest.get_size());

    const VulkanBufferResource& native_source = dynamic_cast<const VulkanBufferResource&>(source);
    const VulkanBufferResource& native_dest = dynamic_cast<const VulkanBufferResource&>(dest);

    VkBufferCopy copy{};
    copy.srcOffset = 0;
    copy.dstOffset = 0;
    copy.size = source.get_size();

    vkCmdCopyBuffer(m_command_buffer, native_source.handle(), native_dest.handle(), 1, &copy);
}

void VulkanCommandBuffer::copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const
{
    const VulkanImageResource& native_image = dynamic_cast<const VulkanImageResource&>(image);
    const VulkanBufferResource& native_buffer = dynamic_cast<const VulkanBufferResource&>(buffer);

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

    const VulkanAccelerationStructure& native_blas = dynamic_cast<const VulkanAccelerationStructure&>(blas);
    const VulkanBufferResource& native_scratch_buffer = dynamic_cast<const VulkanBufferResource&>(scratch_buffer);

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

        const VulkanAccelerationStructure& native_blas = dynamic_cast<const VulkanAccelerationStructure&>(*data.blas);
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
    const VulkanAccelerationStructure& native_tlas = dynamic_cast<const VulkanAccelerationStructure&>(tlas);
    const VulkanBufferResource& native_scratch_buffer = dynamic_cast<const VulkanBufferResource&>(scratch_buffer);

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
    const VulkanAccelerationStructure& native_tlas = dynamic_cast<const VulkanAccelerationStructure&>(tlas);
    const VulkanBufferResource& native_scratch_buffer = dynamic_cast<const VulkanBufferResource&>(scratch_buffer);

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info = native_tlas.get_build_geometry_info();
    VkAccelerationStructureBuildRangeInfoKHR build_range_info = native_tlas.get_build_range_info();

    build_geometry_info.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    build_geometry_info.srcAccelerationStructure = native_tlas.handle();

    build_tlas_internal(
        m_command_buffer, native_tlas, build_geometry_info, build_range_info, instances, native_scratch_buffer);
}

void VulkanCommandBuffer::begin_gpu_marker(const std::string_view& label) const
{
    VK_DEBUG_BEGIN_GPU_MARKER(m_command_buffer, label);
}

void VulkanCommandBuffer::end_gpu_marker() const
{
    VK_DEBUG_END_GPU_MARKER(m_command_buffer);
}

void VulkanCommandBuffer::clear_bound_resource_groups()
{
    for (ResourceGroupInfo& info : m_bound_resource_groups)
    {
        info.resource_group = nullptr;
    }
}

std::shared_ptr<VulkanQueue> VulkanCommandBuffer::get_queue() const
{
    switch (m_type)
    {
    case CommandBufferType::Graphics:
        return VulkanContext.device->get_graphics_queue();
    case CommandBufferType::Compute:
        return VulkanContext.device->get_compute_queue();
    case CommandBufferType::Transfer:
        return VulkanContext.device->get_transfer_queue();
    }

    MIZU_UNREACHABLE("Invalid CommandBufferType");
}

} // namespace Mizu::Vulkan
