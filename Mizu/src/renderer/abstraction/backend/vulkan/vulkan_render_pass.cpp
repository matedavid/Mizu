#include "vulkan_render_pass.h"

#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_framebuffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu::Vulkan {

static std::vector<VkClearValue> get_clear_values(const std::shared_ptr<VulkanFramebuffer>& framebuffer) {
    std::vector<VkClearValue> clear_values;

    for (const auto& attachment : framebuffer->get_attachments()) {
        VkClearValue clear_value;

        if (ImageUtils::is_depth_format(attachment.image->get_format())) {
            clear_value.depthStencil.depth = attachment.clear_value.r;
        } else {
            const auto& color = attachment.clear_value;
            clear_value.color = {{color.r, color.g, color.b, 1.0f}};
        }

        clear_values.push_back(clear_value);
    }

    return clear_values;
}

VulkanRenderPass::VulkanRenderPass(const Description& desc) : m_description(desc) {
    m_target_framebuffer = std::dynamic_pointer_cast<VulkanFramebuffer>(desc.target_framebuffer);

    m_clear_values = get_clear_values(m_target_framebuffer);

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

void VulkanRenderPass::begin(VkCommandBuffer command_buffer) const {
    vkCmdBeginRenderPass(command_buffer, &m_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void VulkanRenderPass::end(VkCommandBuffer command_buffer) const {
    vkCmdEndRenderPass(command_buffer);
}

} // namespace Mizu::Vulkan