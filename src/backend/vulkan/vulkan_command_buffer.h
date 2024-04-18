#pragma once

#include <functional>
#include <vulkan/vulkan.h>

#include "command_buffer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanQueue;

class IVulkanCommandBuffer : public ICommandBuffer {
  public:
    [[nodiscard]] virtual VkCommandBuffer handle() const = 0;
};

template <CommandBufferType Type>
class VulkanCommandBufferBase : public IVulkanCommandBuffer, public RenderCommandBuffer, public ComputeCommandBuffer {
  public:
    VulkanCommandBufferBase();
    ~VulkanCommandBufferBase() override;

    void begin() const override;
    void end() const override;

    void submit() const override;
    void submit(const CommandBufferSubmitInfo& info) const override;

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  private:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

using VulkanRenderCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Graphics>;
using VulkanComputeCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Compute>;
using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
