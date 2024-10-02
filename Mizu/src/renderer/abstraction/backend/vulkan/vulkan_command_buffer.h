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
class VulkanComputePipeline;
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

    void transition_resource(const std::shared_ptr<Texture2D>& texture,
                             ImageResourceState old_state,
                             ImageResourceState new_state) const override;

    void begin_debug_label(const std::string& label) const override;
    void end_debug_label() const override;

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    std::map<uint32_t, std::shared_ptr<VulkanResourceGroup>> m_bound_resources;

    void bind_bound_resources(const std::shared_ptr<VulkanShaderBase>& shader, VkPipelineBindPoint bind_point) const;

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

//
// VulkanRenderCommandBuffer
//

class VulkanRenderCommandBuffer : public RenderCommandBuffer,
                                  public virtual VulkanCommandBufferBase<CommandBufferType::Graphics> {
  public:
    VulkanRenderCommandBuffer() = default;
    ~VulkanRenderCommandBuffer() override = default;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) override;

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;
    void bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) override;

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void begin_render_pass(const std::shared_ptr<VulkanRenderPass>& render_pass,
                           const std::shared_ptr<VulkanFramebuffer>& framebuffer);
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;

    void dispatch(glm::uvec3 group_count) override;

  private:
    std::shared_ptr<VulkanGraphicsPipeline> m_bound_graphics_pipeline{nullptr};
    std::shared_ptr<VulkanComputePipeline> m_bound_compute_pipeline{nullptr};
};

//
// VulkanComputeCommandBuffer
//

class VulkanComputeCommandBuffer : public ComputeCommandBuffer,
                                   public virtual VulkanCommandBufferBase<CommandBufferType::Compute> {
  public:
    VulkanComputeCommandBuffer() = default;
    ~VulkanComputeCommandBuffer() override = default;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) override;

    void bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) override;
    void dispatch(glm::uvec3 group_count) override;

  private:
    std::shared_ptr<VulkanComputePipeline> m_bound_pipeline{nullptr};
};

//
// VulkanTransferCommandBuffer
//

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
