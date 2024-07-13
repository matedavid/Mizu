#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/render_pass.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanFramebuffer;

class VulkanRenderPass : public RenderPass {
  public:
    explicit VulkanRenderPass(const Description& desc);
    ~VulkanRenderPass() override = default;

    void begin(VkCommandBuffer command_buffer) const;
    void begin(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const;
    void end(VkCommandBuffer command_buffer) const;

    [[nodiscard]] std::shared_ptr<VulkanFramebuffer> get_target_framebuffer() const { return m_target_framebuffer; }

  private:
    Description m_description;
    VkRenderPassBeginInfo m_begin_info{};

    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;
    std::vector<VkClearValue> m_clear_values;
};

} // namespace Mizu::Vulkan
