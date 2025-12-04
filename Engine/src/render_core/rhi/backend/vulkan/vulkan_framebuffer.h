#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "render_core/rhi/framebuffer.h"

namespace Mizu::Vulkan
{

class VulkanFramebuffer : public Framebuffer
{
  public:
    explicit VulkanFramebuffer(FramebufferDescription desc);
    VulkanFramebuffer(FramebufferDescription desc, VkRenderPass render_pass);
    ~VulkanFramebuffer() override;

    std::span<const FramebufferAttachment> get_color_attachments() const override
    {
        return std::span<const FramebufferAttachment>(
            m_description.color_attachments.data(), m_description.color_attachments.size());
    }

    std::optional<const FramebufferAttachment> get_depth_stencil_attachment() const override
    {
        return m_description.depth_stencil_attachment;
    }

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }

    VkFramebuffer handle() const { return m_framebuffer; }
    VkRenderPass get_render_pass() const { return m_render_pass; }
    std::span<const VkClearValue> get_clear_values() const
    {
        return std::span(m_clear_values.data(), m_clear_values.size());
    }

  private:
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    VkRenderPass m_render_pass{VK_NULL_HANDLE};

    inplace_vector<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> m_clear_values;

    bool m_owns_render_pass = true;

    FramebufferDescription m_description;

    void create_render_pass();
    void create_framebuffer();
    void create_clear_values();

    static VkAttachmentLoadOp get_load_op(LoadOperation op);
    static VkAttachmentStoreOp get_store_op(StoreOperation op);
};

} // namespace Mizu::Vulkan
