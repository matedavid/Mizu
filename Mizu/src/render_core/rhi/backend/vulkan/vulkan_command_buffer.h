#pragma once

#include <functional>
#include <map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/command_buffer.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanQueue;
class VulkanResourceGroup;
class VulkanShaderBase;
class VulkanGraphicsPipeline;
class VulkanComputePipeline;
class VulkanFramebuffer;
class VulkanRenderPass;

class IVulkanCommandBuffer : public virtual ICommandBuffer
{
  public:
    [[nodiscard]] virtual VkCommandBuffer handle() const = 0;
};

template <CommandBufferType Type>
class VulkanCommandBufferBase : public IVulkanCommandBuffer
{
  public:
    VulkanCommandBufferBase();
    virtual ~VulkanCommandBufferBase() override;

    void begin() override;
    void end() override;

    void submit() const override;
    void submit(const CommandBufferSubmitInfo& info) const override;

    void bind_resource_group(const ResourceGroup& resource_group, uint32_t set) const override;
    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void transition_resource(ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state) const override;

    void begin_debug_label(const std::string_view& label) const override;
    void end_debug_label() const override;

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    std::shared_ptr<VulkanShaderBase> m_currently_bound_shader{nullptr};

    [[nodiscard]] static std::shared_ptr<VulkanQueue> get_queue();
};

//
// VulkanRenderCommandBuffer
//

class VulkanRenderCommandBuffer : public RenderCommandBuffer,
                                  public virtual VulkanCommandBufferBase<CommandBufferType::Graphics>
{
  public:
    VulkanRenderCommandBuffer() = default;
    ~VulkanRenderCommandBuffer() override = default;

    void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;

    void begin_render_pass(const RenderPass& render_pass) const override;
    void begin_render_pass(const VulkanRenderPass& render_pass, const VulkanFramebuffer& framebuffer) const;
    void end_render_pass(const RenderPass& render_pass) const override;

    void draw(const VertexBuffer& vertex) const override;
    void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const override;

    void dispatch(glm::uvec3 group_count) const override;
};

//
// VulkanComputeCommandBuffer
//

class VulkanComputeCommandBuffer : public ComputeCommandBuffer,
                                   public virtual VulkanCommandBufferBase<CommandBufferType::Compute>
{
  public:
    VulkanComputeCommandBuffer() = default;
    ~VulkanComputeCommandBuffer() override = default;

    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;
    void dispatch(glm::uvec3 group_count) const override;
};

//
// VulkanTransferCommandBuffer
//

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
