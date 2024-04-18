#pragma once

#include <vector>
#include <vulkan/vulkan.h>

#include "render_pass.h"

namespace Mizu::Vulkan {

class VulkanRenderPass : public RenderPass {
  public:
    explicit VulkanRenderPass(const Description& desc);
    ~VulkanRenderPass() override = default;

    void begin(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    void end(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;

  private:
    Description m_description;
    VkRenderPassBeginInfo m_begin_info{};

    std::vector<VkClearValue> m_clear_values;
};

} // namespace Mizu::Vulkan
