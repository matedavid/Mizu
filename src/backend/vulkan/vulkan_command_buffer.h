#pragma once

#include <vulkan/vulkan.h>

#include "command_buffer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanQueue;

template <CommandBufferType Type>
class VulkanCommandBufferBase : public RenderCommandBuffer, public ComputeCommandBuffer {
  public:
    VulkanCommandBufferBase();
    ~VulkanCommandBufferBase() override;

    void begin() const override;
    void end() const override;

    void submit() const override;
    void submit(const CommandBufferSubmitInfo& info) const override;

  private:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};

    [[nodiscard]] std::shared_ptr<VulkanQueue> get_queue() const;
};

using VulkanRenderCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Graphics>;
using VulkanComputeCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Compute>;

} // namespace Mizu::Vulkan
