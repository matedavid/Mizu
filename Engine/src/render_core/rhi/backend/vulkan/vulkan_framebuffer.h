#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "render_core/rhi/framebuffer.h"

namespace Mizu::Vulkan
{

class VulkanFramebuffer : public Framebuffer
{
  public:
    explicit VulkanFramebuffer(Description desc);
    VulkanFramebuffer(Description desc, VkRenderPass render_pass);
    ~VulkanFramebuffer() override;

    std::vector<Attachment> get_attachments() const override { return m_description.attachments; };

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }

    VkFramebuffer handle() const { return m_framebuffer; }
    VkRenderPass get_render_pass() const { return m_render_pass; }

  private:
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    VkRenderPass m_render_pass{VK_NULL_HANDLE};

    bool m_owns_render_pass = true;

    Description m_description;

    void create_render_pass();
    void create_framebuffer();

    static VkAttachmentLoadOp get_load_op(LoadOperation op);
    static VkAttachmentStoreOp get_store_op(StoreOperation op);
};

} // namespace Mizu::Vulkan
