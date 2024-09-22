#pragma once

#include <functional>
#include <map>
#include <vulkan/vulkan.h>

#include "renderer/abstraction/command_buffer.h"

namespace Mizu::Vulkan {

// Forward declarations
class VulkanQueue;
class VulkanResourceGroup;
class VulkanShaderBase;
class VulkanGraphicsPipeline;
class VulkanFramebuffer;
class VulkanRenderPass;

class IVulkanCommandBuffer : public virtual ICommandBuffer {
  public:
    [[nodiscard]] virtual VkCommandBuffer handle() const = 0;
};

template <CommandBufferType Type>
class VulkanCommandBufferBase : public IVulkanCommandBuffer {
  public:
    VulkanCommandBufferBase();
    virtual ~VulkanCommandBufferBase() override;

    void begin() override;
    void end() override;

    void submit() const override;
    void submit(const CommandBufferSubmitInfo& info) const override;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    virtual void push_constant([[maybe_unused]] std::string_view name,
                               [[maybe_unused]] uint32_t size,
                               [[maybe_unused]] const void* data) override {}

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    std::map<uint32_t, std::shared_ptr<VulkanResourceGroup>> m_bound_resources;

    void bind_bound_resources(const std::shared_ptr<VulkanShaderBase>& shader) const;

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

//
// VulkanRenderCommandBuffer
//

class VulkanRenderCommandBuffer : public RenderCommandBuffer,
                                  public VulkanCommandBufferBase<CommandBufferType::Graphics> {
  public:
    VulkanRenderCommandBuffer() = default;
    ~VulkanRenderCommandBuffer() override = default;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) override;

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void begin_render_pass(const std::shared_ptr<VulkanRenderPass>& render_pass,
                           const std::shared_ptr<VulkanFramebuffer>& framebuffer);
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;

  private:
    std::shared_ptr<VulkanGraphicsPipeline> m_bound_pipeline{nullptr};
};

//
// VulkanComputeCommandBuffer
//

// class VulkanComputeCommandBuffer : public ComputeCommandBuffer,
//                                    public VulkanCommandBufferBase<CommandBufferType::Compute> {
//   public:
//     VulkanComputeCommandBuffer() = default;
//     ~VulkanComputeCommandBuffer() override = default;
//
//     void begin() override { begin_base(); }
//     void end() override { end_base(); }
//
//     void submit() const override { submit_base(); }
//     void submit(const CommandBufferSubmitInfo& info) const override { submit_base(info); }
//
//     void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override {
//         /* TODO */
//         bind_resource_group_base(resource_group, set);
//     }
//
//     void push_constant(std::string_view name, uint32_t size, const void* data) override { /* TODO */ }
// };

//
// VulkanTransferCommandBuffer
//

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
