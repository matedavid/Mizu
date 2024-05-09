#pragma once

#include <functional>
#include <map>
#include <vulkan/vulkan.h>

#include "command_buffer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanQueue;
class VulkanResourceGroup;

class IVulkanCommandBuffer {
  public:
    [[nodiscard]] virtual VkCommandBuffer handle() const = 0;
};

template <CommandBufferType Type>
class VulkanCommandBufferBase : public IVulkanCommandBuffer {
  public:
    VulkanCommandBufferBase();
    ~VulkanCommandBufferBase();

    void begin_base();
    void end_base();

    void submit_base() const;
    void submit_base(const CommandBufferSubmitInfo& info) const;

    void bind_resource_group_base(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set);

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};

    std::map<uint32_t, std::shared_ptr<VulkanResourceGroup>> m_bound_resources;

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

class VulkanRenderCommandBuffer : public RenderCommandBuffer,
                                  public VulkanCommandBufferBase<CommandBufferType::Graphics> {
  public:
    VulkanRenderCommandBuffer() = default;
    ~VulkanRenderCommandBuffer() override = default;

    void begin() override { begin_base(); }
    void end() override { end_base(); }

    void submit() const override { submit_base(); }
    void submit(const CommandBufferSubmitInfo& info) const override { submit_base(info); }

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override {
        bind_resource_group_base(resource_group, set);
    }

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;
};

class VulkanComputeCommandBuffer : public ComputeCommandBuffer,
                                   public VulkanCommandBufferBase<CommandBufferType::Compute> {
  public:
    VulkanComputeCommandBuffer() = default;
    ~VulkanComputeCommandBuffer() override = default;

    void begin() override { begin_base(); }
    void end() override { end_base(); }

    void submit() const override { submit_base(); }
    void submit(const CommandBufferSubmitInfo& info) const override { submit_base(info); }

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override {
        bind_resource_group_base(resource_group, set);
    }
};

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
