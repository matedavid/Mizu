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

    // TODO: Max number of resources should be extracted from device capabilities
    m_resources.resize(4);
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
    for (size_t i = 0; i < m_resources.size(); ++i)
    {
        m_resources[i] = CommandBufferResourceGroup{};
    }

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
void VulkanCommandBufferBase<Type>::bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set)
{
    MIZU_ASSERT(set < m_resources.size(),
                "Set value is bigger than max number of resource groups ({} >= {})",
                set,
                m_resources.size());

    const auto native_resource_group = std::dynamic_pointer_cast<VulkanResourceGroup>(resource_group);
    if (m_currently_bound_shader == nullptr)
    {
        if (m_resources[set].has_value() && m_resources[set].resource_group->get_hash() == resource_group->get_hash())
        {
            return;
        }

        CommandBufferResourceGroup rg{};
        rg.resource_group = native_resource_group;
        rg.set = set;
        rg.is_bound = false;

        m_resources[set] = rg;
    }
    else
    {
        const std::optional<VkDescriptorSetLayout>& shader_layout =
            m_currently_bound_shader->get_descriptor_set_layout(set);
        MIZU_ASSERT(shader_layout.has_value(), "Shader does not contain descriptor set {}", set);

        MIZU_ASSERT(*shader_layout == native_resource_group->get_descriptor_set_layout(),
                    "Shader layout at set {} is not compatible with resource group layout",
                    set);

        if (m_resources[set].has_value()
            && native_resource_group->get_hash() == m_resources[set].resource_group->get_hash())
        {
            // Is same resource, no need to bind
            return;
        }

        CommandBufferResourceGroup rg{};
        rg.resource_group = native_resource_group;
        rg.set = set;
        rg.is_bound = true;

        m_resources[set] = rg;

        bind_resource_group_internal(*native_resource_group, m_currently_bound_shader->get_pipeline_layout(), set);
    }
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
    transition_resource(image, old_state, new_state, {0, image.get_num_mips()}, {0, image.get_num_layers()});
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::transition_resource(ImageResource& image,
                                                        ImageResourceState old_state,
                                                        ImageResourceState new_state,
                                                        std::pair<uint32_t, uint32_t> mip_range,
                                                        std::pair<uint32_t, uint32_t> layer_range) const
{
    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const VulkanImageResource& native_image = dynamic_cast<VulkanImageResource&>(image);

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
    barrier.subresourceRange.baseMipLevel = mip_range.first;
    barrier.subresourceRange.levelCount = mip_range.second;
    barrier.subresourceRange.baseArrayLayer = layer_range.first;
    barrier.subresourceRange.layerCount = layer_range.second;

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

        DEFINE_TRANSITION(ShaderReadOnly,
                          DepthStencilAttachment,
                          VK_ACCESS_SHADER_READ_BIT,
                          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT),

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
                          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT),
    };

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
    const auto fence = Fence::create();
    fence->wait_for(); // Fences start signaled by default

    VulkanCommandBufferBase<Type> command_buffer{};

    command_buffer.begin();

    func(command_buffer);

    command_buffer.end();

    CommandBufferSubmitInfo submit_info{};
    submit_info.signal_fence = fence;

    command_buffer.submit(submit_info);

    fence->wait_for();
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::bind_resources(const VulkanShaderBase& new_shader)
{
    for (CommandBufferResourceGroup& rg_info : m_resources)
    {
        if (rg_info.has_value() && !rg_info.is_bound)
        {
            if (!rg_info.resource_group->is_baked())
            {
                [[maybe_unused]] const bool baked = rg_info.resource_group->bake(new_shader, rg_info.set);
                MIZU_VERIFY(baked, "Error baking resource group with set {}", rg_info.set);
            }

            bind_resource_group_internal(*rg_info.resource_group, new_shader.get_pipeline_layout(), rg_info.set);

            rg_info.is_bound = true;
        }
        else if (rg_info.has_value() && rg_info.is_bound && m_previous_shader != nullptr)
        {
            const std::optional<VkDescriptorSetLayout>& layout = new_shader.get_descriptor_set_layout(rg_info.set);
            if (!layout.has_value() || m_previous_shader->get_descriptor_set_layout(rg_info.set) != *layout)
            {
                m_resources[rg_info.set] = CommandBufferResourceGroup{};
            }
            else if (layout.has_value() && m_previous_shader->get_descriptor_set_layout(rg_info.set) == *layout)
            {
                // TODO: Check if this is really necessary, I thought that if a descriptor set layout is compatible with
                // a previous pipeline state, i should not need to rebind the descriptor set
                bind_resource_group_internal(*rg_info.resource_group, new_shader.get_pipeline_layout(), rg_info.set);
            }
        }
    }
}

template <CommandBufferType Type>
void VulkanCommandBufferBase<Type>::bind_resource_group_internal(const VulkanResourceGroup& resource_group,
                                                                 VkPipelineLayout pipeline_layout,
                                                                 uint32_t set) const
{
    MIZU_ASSERT(m_pipeline_bind_point != VK_PIPELINE_BIND_POINT_MAX_ENUM, "Bind point not valid");

    const VkDescriptorSet& descriptor_set = resource_group.get_descriptor_set();
    vkCmdBindDescriptorSets(
        m_command_buffer, m_pipeline_bind_point, pipeline_layout, set, 1, &descriptor_set, 0, nullptr);
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

void VulkanRenderCommandBuffer::begin_render_pass(std::shared_ptr<RenderPass> render_pass)
{
    const auto& native_render_pass = std::dynamic_pointer_cast<VulkanRenderPass>(render_pass);
    const VulkanFramebuffer& native_framebuffer =
        dynamic_cast<const VulkanFramebuffer&>(*render_pass->get_framebuffer());

    begin_render_pass(native_render_pass, native_framebuffer);
}

void VulkanRenderCommandBuffer::begin_render_pass(std::shared_ptr<VulkanRenderPass> render_pass,
                                                  const VulkanFramebuffer& framebuffer)
{
    MIZU_ASSERT(m_bound_render_pass == nullptr, "Can't bind RenderPass because a RenderPass has already been bound");

    m_bound_render_pass = std::move(render_pass);
    m_bound_render_pass->begin(m_command_buffer, framebuffer.handle());

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

void VulkanRenderCommandBuffer::end_render_pass()
{
    MIZU_ASSERT(m_bound_render_pass != nullptr, "Can't end RenderPass because no RenderPass is bound");

    m_bound_render_pass->end(m_command_buffer);
    m_bound_render_pass = nullptr;

    m_bound_graphics_pipeline = nullptr;
    m_bound_compute_pipeline = nullptr;
    m_currently_bound_shader = nullptr;
}

void VulkanRenderCommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    MIZU_ASSERT(m_bound_render_pass != nullptr, "Can't bind pipeline because no RenderPass is bound");

    m_bound_graphics_pipeline = std::dynamic_pointer_cast<VulkanGraphicsPipeline>(pipeline);
    m_bound_compute_pipeline = nullptr;

    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bound_graphics_pipeline->handle());
    m_pipeline_bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;

    bind_resources(*m_bound_graphics_pipeline->get_shader());
    m_currently_bound_shader = m_bound_graphics_pipeline->get_shader();
    m_previous_shader = m_currently_bound_shader;
}

void VulkanRenderCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    MIZU_ASSERT(m_bound_render_pass == nullptr, "Can't bind compute pipeline because a render pass is bound");

    m_bound_graphics_pipeline = nullptr;
    m_bound_compute_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);

    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bound_compute_pipeline->handle());
    m_pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    bind_resources(*m_bound_compute_pipeline->get_shader());
    m_currently_bound_shader = m_bound_compute_pipeline->get_shader();
    m_previous_shader = m_currently_bound_shader;
}

void VulkanRenderCommandBuffer::draw(const VertexBuffer& vertex) const
{
    draw_instanced(vertex, 1);
}

void VulkanRenderCommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    draw_indexed_instanced(vertex, index, 1);
}

void VulkanRenderCommandBuffer::draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const
{
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr, "Can't draw because no GraphicsPipeline has been bound");

    const auto& native_buffer = std::dynamic_pointer_cast<VulkanBufferResource>(vertex.get_resource());

    const std::array<VkBuffer, 1> vertex_buffers = {native_buffer->handle()};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(
        m_command_buffer, 0, static_cast<uint32_t>(vertex_buffers.size()), vertex_buffers.data(), offsets);

    vkCmdDraw(m_command_buffer, vertex.get_count(), instance_count, 0, 0);
}

void VulkanRenderCommandBuffer::draw_indexed_instanced(const VertexBuffer& vertex,
                                                       const IndexBuffer& index,
                                                       uint32_t instance_count) const
{
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr, "Can't draw indexed because no GraphicsPipeline has been bound");

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

void VulkanRenderCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(m_bound_compute_pipeline != nullptr,
                "To call dispatch on RenderCommandBuffer you must have previously bound a ComputePipeline");

    vkCmdDispatch(m_command_buffer, group_count.x, group_count.y, group_count.z);
}

//
// VulkanComputeCommandBuffer
//

void VulkanComputeCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    m_bound_pipeline = std::dynamic_pointer_cast<VulkanComputePipeline>(pipeline);

    vkCmdBindPipeline(m_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_bound_pipeline->handle());
    m_pipeline_bind_point = VK_PIPELINE_BIND_POINT_COMPUTE;

    bind_resources(*m_bound_pipeline->get_shader());
    m_currently_bound_shader = m_bound_pipeline->get_shader();
    m_previous_shader = m_currently_bound_shader;
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
