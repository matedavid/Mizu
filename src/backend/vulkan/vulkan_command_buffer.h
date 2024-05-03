#pragma once

#include <functional>
#include <vulkan/vulkan.h>

#include "command_buffer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanQueue;

class IVulkanCommandBuffer {
  public:
    [[nodiscard]] virtual VkCommandBuffer handle() const = 0;
};

template <CommandBufferType Type>
class VulkanCommandBufferBase : public IVulkanCommandBuffer {
  public:
    VulkanCommandBufferBase();
    ~VulkanCommandBufferBase();

    void begin_base() const;
    void end_base() const;

    void submit_base() const;
    void submit_base(const CommandBufferSubmitInfo& info) const;

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

class VulkanRenderCommandBuffer : public RenderCommandBuffer,
                                  public VulkanCommandBufferBase<CommandBufferType::Graphics> {
  public:
    VulkanRenderCommandBuffer() = default;
    ~VulkanRenderCommandBuffer() override = default;

    void begin() const override { begin_base(); }
    void end() const override { end_base(); }

    void submit() const override { submit_base(); }
    void submit(const CommandBufferSubmitInfo& info) const override { submit_base(info); }

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;
};

class VulkanComputeCommandBuffer : public ComputeCommandBuffer,
                                   public VulkanCommandBufferBase<CommandBufferType::Compute> {
  public:
    VulkanComputeCommandBuffer() = default;
    ~VulkanComputeCommandBuffer() override = default;

    void begin() const override { begin_base(); }
    void end() const override { end_base(); }

    void submit() const override { submit_base(); }
    void submit(const CommandBufferSubmitInfo& info) const override { submit_base(info); }
};

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
