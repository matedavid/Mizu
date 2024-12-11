#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include "renderer/abstraction/framebuffer.h"

namespace Mizu::Vulkan
{

class VulkanFramebuffer : public Framebuffer
{
  public:
    explicit VulkanFramebuffer(const Description& desc);
    VulkanFramebuffer(const Description& desc, VkRenderPass render_pass);
    ~VulkanFramebuffer() override;

    [[nodiscard]] std::vector<Attachment> get_attachments() const override { return m_description.attachments; };

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }

    [[nodiscard]] VkFramebuffer handle() const { return m_framebuffer; }
    [[nodiscard]] VkRenderPass get_render_pass() const { return m_render_pass; }

  private:
    VkFramebuffer m_framebuffer{VK_NULL_HANDLE};
    VkRenderPass m_render_pass{VK_NULL_HANDLE};

    bool m_owns_render_pass = true;

    Description m_description;

    void create_render_pass();
    void create_framebuffer();

    [[nodiscard]] static VkAttachmentLoadOp get_load_op(LoadOperation op);
    [[nodiscard]] static VkAttachmentStoreOp get_store_op(StoreOperation op);
};

} // namespace Mizu::Vulkan
