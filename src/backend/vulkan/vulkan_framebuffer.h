#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "framebuffer.h"

namespace Mizu::Vulkan {

class VulkanFramebuffer : public Framebuffer {
  public:
    explicit VulkanFramebuffer(const Description& desc);
    ~VulkanFramebuffer() override;

    [[nodiscard]] std::vector<Attachment> get_attachments() const override { return m_description.attachments; };

    [[nodiscard]] VkRenderPass get_render_pass() const { return m_render_pass; }

  private:
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    VkRenderPass m_render_pass{VK_NULL_HANDLE};

    Description m_description;

    [[nodiscard]] static VkAttachmentLoadOp get_load_op(LoadOperation op);
    [[nodiscard]] static VkAttachmentStoreOp get_store_op(StoreOperation op);
};

} // namespace Mizu::Vulkan