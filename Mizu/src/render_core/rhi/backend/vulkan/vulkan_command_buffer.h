#pragma once

#include <functional>
#include <map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/backend/vulkan/vulkan_render_pass.h"

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

    void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void transition_resource(ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state) const override;
    void transition_resource(ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state,
                             ImageResourceViewRange range) const override;

    void copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const override;
    void copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const override;

    void begin_debug_label(const std::string_view& label) const override;
    void end_debug_label() const override;

    static void submit_single_time(const std::function<void(const VulkanCommandBufferBase<Type>&)>& func);

    [[nodiscard]] VkCommandBuffer handle() const override { return m_command_buffer; }

  protected:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    std::shared_ptr<VulkanShaderBase> m_currently_bound_shader{nullptr};
    std::shared_ptr<VulkanShaderBase> m_previous_shader{nullptr};
    VkPipelineBindPoint m_pipeline_bind_point = VK_PIPELINE_BIND_POINT_MAX_ENUM;

    struct CommandBufferResourceGroup
    {
        std::shared_ptr<VulkanResourceGroup> resource_group = nullptr;
        uint32_t set = 0;
        bool is_bound = false;

        bool has_value() const { return resource_group != nullptr; }
    };
    std::vector<CommandBufferResourceGroup> m_resources;

    void bind_resources(const VulkanShaderBase& new_shader);
    void bind_resource_group_internal(const VulkanResourceGroup& resource_group,
                                      VkPipelineLayout pipeline_layout,
                                      uint32_t set) const;

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

    void begin_render_pass(std::shared_ptr<RenderPass> render_pass) override;
    void begin_render_pass(std::shared_ptr<VulkanRenderPass> render_pass, const VulkanFramebuffer& framebuffer);
    void end_render_pass() override;

    void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;

    void draw(const VertexBuffer& vertex) const override;
    void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const override;

    void draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const override;
    void draw_indexed_instanced(const VertexBuffer& vertex,
                                const IndexBuffer& index,
                                uint32_t instance_count) const override;

    void dispatch(glm::uvec3 group_count) const override;

    [[nodiscard]] std::shared_ptr<RenderPass> get_current_render_pass() const override
    {
        return std::dynamic_pointer_cast<RenderPass>(m_bound_render_pass);
    }

  private:
    std::shared_ptr<VulkanRenderPass> m_bound_render_pass{};

    std::shared_ptr<VulkanGraphicsPipeline> m_bound_graphics_pipeline{};
    std::shared_ptr<VulkanComputePipeline> m_bound_compute_pipeline{};
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

  private:
    std::shared_ptr<VulkanComputePipeline> m_bound_pipeline{};
};

//
// VulkanTransferCommandBuffer
//

using VulkanTransferCommandBuffer = VulkanCommandBufferBase<CommandBufferType::Transfer>;

} // namespace Mizu::Vulkan
