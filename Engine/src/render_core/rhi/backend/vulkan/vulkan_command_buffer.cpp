#include "vulkan_command_buffer.h"

#include <array>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/renderer.h"

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
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"
#include "render_core/rhi/backend/vulkan/vulkan_shader.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

#include "render_core/rhi/backend/vulkan/rtx/vulkan_acceleration_structure.h"
#include "render_core/rhi/backend/vulkan/rtx/vulkan_ray_tracing_pipeline.h"
#include "render_core/rhi/backend/vulkan/rtx/vulkan_rtx_core.h"

namespace Mizu::Vulkan
{

VulkanCommandBuffer::VulkanCommandBuffer(CommandBufferType type) : m_type(type)
{
    const std::vector<VkCommandBuffer>& command_buffers = VulkanContext.device->allocate_command_buffers(1, m_type);
    MIZU_ASSERT(!command_buffers.empty() && command_buffers[0] != VK_NULL_HANDLE, "Error allocating command buffers");

    m_command_buffer = command_buffers[0];

    m_bound_resource_groups.resize(Renderer::get_capabilities().max_resource_group_sets);
}

VulkanCommandBuffer::~VulkanCommandBuffer()
{
    VulkanContext.device->free_command_buffers({m_command_buffer}, m_type);
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

    if (info.wait_semaphore != nullptr)
    {
        const auto wait_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.wait_semaphore);
        wait_semaphores.push_back(wait_semaphore->handle());

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

    if (info.signal_semaphore != nullptr)
    {
        const auto signal_semaphore = std::dynamic_pointer_cast<VulkanSemaphore>(info.signal_semaphore);
        signal_semaphores.push_back(signal_semaphore->handle());
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
    MIZU_ASSERT(set < m_bound_resource_groups.size(),
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
    MIZU_ASSERT(
        native_resource_group->get_descriptor_set_layout() == m_bound_pipeline->get_descriptor_set_layout(set),
        "Can't bind resource group because the resource group layout does not match the layout in the pipeline");

    info.resource_group = native_resource_group;
    info.set = set;

    const VkDescriptorSet& descriptor_set = native_resource_group->get_descriptor_set();
    vkCmdBindDescriptorSets(m_command_buffer,
                            m_bound_pipeline->get_pipeline_bind_point(),
                            m_bound_pipeline->get_pipeline_layout(),
                            set,
                            1,
                            &descriptor_set,
                            0,
                            nullptr);
}

void VulkanCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't push constant when no pipeline has been bound");

    const ShaderGroup& shader_group = m_bound_pipeline->get_shader_group();
    const ShaderConstant& constant_info = shader_group.get_constant_info(std::string(name));

    MIZU_ASSERT(constant_info.size == size && size <= Renderer::get_capabilities().max_push_constant_size,
                "Size of push constant does not match expected or is bigger than maximum (size = {}, expected = {}, "
                "maximum = {})",
                size,
                constant_info.size,
                Renderer::get_capabilities().max_push_constant_size);

    const VkShaderStageFlags vk_stage_flags =
        VulkanShader::get_vulkan_shader_stage_bits(shader_group.get_resource_stage_bits(constant_info.name));

    vkCmdPushConstants(m_command_buffer, m_bound_pipeline->get_pipeline_layout(), vk_stage_flags, 0, size, data);
}

void VulkanCommandBuffer::begin_render_pass(std::shared_ptr<RenderPass> render_pass)
{
    MIZU_ASSERT(m_active_render_pass == nullptr, "Can't bind RenderPass because a RenderPass is already active");
    MIZU_ASSERT(m_type == CommandBufferType::Graphics, "Can't begin render pass non Graphics Command Buffer");

    m_active_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    m_active_render_pass->begin(m_command_buffer);

    const uint32_t width = m_active_render_pass->get_framebuffer()->get_width();
    const uint32_t height = m_active_render_pass->get_framebuffer()->get_height();

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

    m_active_render_pass->end(m_command_buffer);
    m_active_render_pass = nullptr;

    // TODO: Investigate if this is necessary. Doing because apparently can't reuse bound resource group in different
    // render passes even if the descriptor set layout is the same.
    clear_bound_resource_groups();
}

void VulkanCommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't bind graphics pipeline because no RenderPass is active");

    m_bound_pipeline = std::dynamic_pointer_cast<VulkanGraphicsPipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bound_pipeline->handle());
}

void VulkanCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    MIZU_ASSERT(m_active_render_pass == nullptr, "Can't bind compute pipeline because a RenderPass is active");

    m_bound_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bound_pipeline->handle());
}

void VulkanCommandBuffer::bind_pipeline(std::shared_ptr<RayTracingPipeline> pipeline)
{
    MIZU_ASSERT(m_active_render_pass == nullptr, "Can't bind ray tracing pipeline because a RenderPass is active");

    m_bound_pipeline = std::dynamic_pointer_cast<VulkanRayTracingPipeline>(pipeline);
    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_bound_pipeline->handle());
}

void VulkanCommandBuffer::draw(const VertexBuffer& vertex) const
{
    draw_instanced(vertex, 1);
}

void VulkanCommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    draw_indexed_instanced(vertex, index, 1);
}

void VulkanCommandBuffer::draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const
{
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't draw_instanced because no RenderPass is active");
    MIZU_ASSERT(m_bound_pipeline != nullptr
                    && m_bound_pipeline->get_pipeline_bind_point() == VK_PIPELINE_BIND_POINT_GRAPHICS,
                "Can't draw_indexed_instance because no graphics pipeline has been bound");

    const auto& native_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(vertex.get_resource());

    const std::array<VkBuffer, 1> vertex_buffers = {native_buffer->handle()};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(
        m_command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets);

    vkCmdDraw(m_command_buffer, vertex.get_count(), instance_count, 0, 0);
}

void VulkanCommandBuffer::draw_indexed_instanced(const VertexBuffer& vertex,
                                                 const IndexBuffer& index,
                                                 uint32_t instance_count) const
{
    MIZU_ASSERT(m_active_render_pass != nullptr, "Can't draw_indexed_instance because no RenderPass is active");
    MIZU_ASSERT(m_bound_pipeline != nullptr
                    && m_bound_pipeline->get_pipeline_bind_point() == VK_PIPELINE_BIND_POINT_GRAPHICS,
                "Can't draw_indexed_instance because no graphics pipeline has been bound");

    {
        const auto& vertex_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(vertex.get_resource());

        const std::array<VkBuffer, 1> vertex_buffers = {vertex_buffer->handle()};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(
            m_command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets);
    }

    {
        const auto& index_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(index.get_resource());
        vkCmdBindIndexBuffer(m_command_buffer, index_buffer->handle(), 0, VK_INDEX_TYPE_UINT32);
    }

    vkCmdDrawIndexed(m_command_buffer, index.get_count(), instance_count, 0, 0, 0);
}

void VulkanCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr
                    && m_bound_pipeline->get_pipeline_bind_point() == VK_PIPELINE_BIND_POINT_COMPUTE,
                "Can't draw_indexed_instance because no compute pipeline has been bound");

    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

void VulkanCommandBuffer::trace_rays(glm::uvec3 dimensions) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr
                    && m_bound_pipeline->get_pipeline_bind_point() == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                "Can't draw_indexed_instance because no ray tracing pipeline has been bound");

    const auto& rtx_pipeline = std::dynamic_pointer_cast<VulkanRayTracingPipeline>(m_bound_pipeline);

    const VkStridedDeviceAddressRegionKHR& raygen_region = rtx_pipeline->get_ray_generation_region();
    const VkStridedDeviceAddressRegionKHR& miss_region = rtx_pipeline->get_miss_region();
    const VkStridedDeviceAddressRegionKHR& hit_region = rtx_pipeline->get_hit_region();
    const VkStridedDeviceAddressRegionKHR& call_region = rtx_pipeline->get_call_region();

    vkCmdTraceRaysKHR(m_command_buffer,
                      &raygen_region,
                      &miss_region,
                      &hit_region,
                      &call_region,
                      dimensions.x,
                      dimensions.y,
                      dimensions.z);
}

void VulkanCommandBuffer::transition_resource(const ImageResource& image,
                                              ImageResourceState old_state,
                                              ImageResourceState new_state) const
{
    const ImageResourceViewRange range =
        ImageResourceViewRange::from_mips_layers(0, image.get_num_mips(), 0, image.get_num_layers());

    transition_resource(image, old_state, new_state, range);
}

void VulkanCommandBuffer::transition_resource(const ImageResource& image,
                                              ImageResourceState old_state,
                                              ImageResourceState new_state,
                                              ImageResourceViewRange range) const
{
    struct TransitionInfo
    {
        VkPipelineStageFlags src_stage, dst_stage;
        VkAccessFlags src_access_mask, dst_access_mask;
    };

#if MIZU_DEBUG
    const auto to_string = [](ImageResourceState state) -> std::string {
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
        case ImageResourceState::Present:
            return "Present";
        }
    };
#endif

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
    barrier.image = native_image.get_image_handle();
    barrier.subresourceRange.aspectMask =
        ImageUtils::is_depth_format(image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = range.get_mip_base();
    barrier.subresourceRange.levelCount = range.get_mip_count();
    barrier.subresourceRange.baseArrayLayer = range.get_layer_base();
    barrier.subresourceRange.layerCount = range.get_layer_count();

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
        DEFINE_TRANSITION(Undefined,
                          General,
                          0,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),
        DEFINE_TRANSITION(Undefined,
                          TransferDst,
                          0,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT),
        DEFINE_TRANSITION(Undefined,
                          ColorAttachment,
                          0,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT),
        DEFINE_TRANSITION(Undefined,
                          DepthStencilAttachment,
                          0,
                          VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),

        // General
        DEFINE_TRANSITION(General,
                          ShaderReadOnly,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        DEFINE_TRANSITION(ShaderReadOnly,
                          DepthStencilAttachment,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT),

        // TransferDst
        DEFINE_TRANSITION(TransferDst,
                          ShaderReadOnly,
                          VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_TRANSFER_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        // ShaderReadOnly
        DEFINE_TRANSITION(ShaderReadOnly,
                          General,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_ALL_COMMANDS_BIT),

        DEFINE_TRANSITION(ShaderReadOnly,
                          Present,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_MEMORY_READ_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),

        // ColorAttachment
        DEFINE_TRANSITION(ColorAttachment,
                          ShaderReadOnly,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT),

        DEFINE_TRANSITION(ColorAttachment,
                          Present,
                          VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                          VK_ACCESS_MEMORY_READ_BIT,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT),

        // DepthStencilAttachment
        DEFINE_TRANSITION(DepthStencilAttachment,
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
        MIZU_LOG_ERROR("Image layout transition not defined: {} -> {} for texture: {}",
                       to_string(old_state),
                       to_string(new_state),
                       native_image.get_name());
        return;
    }

    const TransitionInfo& info = it->second;

    barrier.srcAccessMask = info.src_access_mask;
    barrier.dstAccessMask = info.dst_access_mask;

    vkCmdPipelineBarrier(m_command_buffer, info.src_stage, info.dst_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    MIZU_ASSERT(source.get_size() == dest.get_size(), "Size of buffers do not match");

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
        ImageUtils::is_depth_format(native_image.get_format()) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = native_image.get_num_layers();
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {native_image.get_width(), native_image.get_height(), native_image.get_depth()};

    vkCmdCopyBufferToImage(m_command_buffer,
                           native_buffer.handle(),
                           native_image.get_image_handle(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1,
                           &region);
}

void VulkanCommandBuffer::build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const
{
    MIZU_ASSERT(blas.get_type() == AccelerationStructure::Type::BottomLevel, "Acceleration structure is not BLAS");

    const VulkanAccelerationStructure& native_blas = dynamic_cast<const VulkanAccelerationStructure&>(blas);
    const VulkanBufferResource& native_scratch_buffer = dynamic_cast<const VulkanBufferResource&>(scratch_buffer);

    VkAccelerationStructureBuildGeometryInfoKHR build_geometry_info = native_blas.get_build_geometry_info();
    VkAccelerationStructureBuildRangeInfoKHR build_range_info = native_blas.get_build_range_info();

    build_geometry_info.scratchData.deviceAddress = get_device_address(native_scratch_buffer);

    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &build_range_info;
    vkCmdBuildAccelerationStructuresKHR(m_command_buffer, 1, &build_geometry_info, &build_range);
}

static void build_tlas_internal(VkCommandBuffer command,
                                const VulkanAccelerationStructure& tlas,
                                VkAccelerationStructureBuildGeometryInfoKHR build_geometry,
                                VkAccelerationStructureBuildRangeInfoKHR range_info,
                                std::span<AccelerationStructureInstanceData> instances,
                                const VulkanBufferResource& scratch_buffer)
{
    MIZU_ASSERT(tlas.get_type() == AccelerationStructure::Type::TopLevel, "Acceleration structure is not TLAS");
    MIZU_ASSERT(instances.size() == range_info.primitiveCount,
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
        const VkDeviceAddress blas_address = get_device_address(native_blas);

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

    build_geometry.scratchData.deviceAddress = get_device_address(scratch_buffer);

    const VkAccelerationStructureBuildRangeInfoKHR* build_range = &range_info;
    vkCmdBuildAccelerationStructuresKHR(command, 1, &build_geometry, &build_range);
}

void VulkanCommandBuffer::build_tlas(const AccelerationStructure& tlas,
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

void VulkanCommandBuffer::update_tlas(const AccelerationStructure& tlas,
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

void VulkanCommandBuffer::begin_debug_label(const std::string_view& label) const
{
    VK_DEBUG_BEGIN_LABEL(m_command_buffer, label);
}

void VulkanCommandBuffer::end_debug_label() const
{
    VK_DEBUG_END_LABEL(m_command_buffer);
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
