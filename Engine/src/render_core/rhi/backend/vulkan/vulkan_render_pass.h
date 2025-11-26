#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "base/containers/inplace_vector.h"

#include "render_core/rhi/framebuffer.h"
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

    std::shared_ptr<Framebuffer> get_framebuffer() const override;

  private:
    VkRenderPassBeginInfo m_begin_info{};

    std::shared_ptr<VulkanFramebuffer> m_target_framebuffer;
    inplace_vector<VkClearValue, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS + 1> m_clear_values;
};

} // namespace Mizu::Vulkan
