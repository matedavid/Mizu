#include "vulkan_render_pass.h"

#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_resource_view.h"

namespace Mizu::Vulkan
{

static inplace_vector<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> get_clear_values(
    const VulkanFramebuffer& framebuffer)
{
    inplace_vector<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> clear_values;

    for (const Framebuffer::Attachment& attachment : framebuffer.get_color_attachments())
    {
        VkClearValue clear_value{};

        const glm::vec4& color = attachment.clear_value;
        clear_value.color = {{color.r, color.g, color.b, color.a}};

        clear_values.push_back(clear_value);
    }

    const std::optional<const Framebuffer::Attachment>& depth_stencil_attachment_opt =
        framebuffer.get_depth_stencil_attachment();
    if (depth_stencil_attachment_opt.has_value())
    {
        const Framebuffer::Attachment& attachment = *depth_stencil_attachment_opt;

        clear_values.push_back(VkClearValue{
            .depthStencil =
                VkClearDepthStencilValue{
                    .depth = attachment.clear_value.r,
                    .stencil = 0,
                },
        });
    }

    return clear_values;
}

VulkanRenderPass::VulkanRenderPass(const Description& desc)
{
    m_target_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);

    m_clear_values = get_clear_values(*m_target_framebuffer);

    m_begin_info = VkRenderPassBeginInfo{};
    m_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    m_begin_info.renderPass = m_target_framebuffer->get_render_pass();
    m_begin_info.framebuffer = m_target_framebuffer->handle();
    m_begin_info.renderArea.offset = {0, 0};
    m_begin_info.renderArea.extent = {
        m_target_framebuffer->get_width(),
        m_target_framebuffer->get_height(),
    };
    m_begin_info.clearValueCount = static_cast<uint32_t>(m_clear_values.size());
    m_begin_info.pClearValues = m_clear_values.data();
}

void VulkanRenderPass::begin(VkCommandBuffer command_buffer) const
{
    begin(command_buffer, m_begin_info.framebuffer);
}

void VulkanRenderPass::begin(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const
{
    VkRenderPassBeginInfo info = m_begin_info;
    info.framebuffer = framebuffer;

    vkCmdBeginRenderPass(command_buffer, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderPass::end(VkCommandBuffer command_buffer) const
{
    vkCmdEndRenderPass(command_buffer);
}

std::shared_ptr<Framebuffer> VulkanRenderPass::get_framebuffer() const
{
    return std::dynamic_pointer_cast<Framebuffer>(m_target_framebuffer);
}

} // namespace Mizu::Vulkan
