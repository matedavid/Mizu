#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_core/rhi/render_pass.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanFramebuffer;

class VulkanRenderPass : public RenderPass
{
  public:
    explicit VulkanRenderPass(const Description& desc);
    ~VulkanRenderPass() override = default;

    void begin(VkCommandBuffer command_buffer) const;
    void begin(VkCommandBuffer command_buffer, VkFramebuffer framebuffer) const;
    void end(VkCommandBuffer command_buffer) const;

    [[nodiscard]] std::shared_ptr<Framebuffer> get_framebuffer() const override;

  private:
    VkRenderPassBeginInfo m_begin_info{};

    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;
    std::vector<VkClearValue> m_clear_values;
};

} // namespace Mizu::Vulkan
