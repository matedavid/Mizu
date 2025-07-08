#pragma once

#include <functional>
#include <map>
#include <vulkan/vulkan.h>

#include "render_core/rhi/command_buffer.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/vulkan/vulkan_render_pass.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanQueue;
class VulkanResourceGroup;
class VulkanShaderBase;
class VulkanGraphicsPipeline;
class VulkanComputePipeline;
class VulkanRayTracingPipeline;
class VulkanFramebuffer;
class VulkanRenderPass;
class IVulkanPipeline;

class VulkanCommandBuffer : public CommandBuffer
{
  public:
    VulkanCommandBuffer(CommandBufferType type);
    ~VulkanCommandBuffer() override;

    void begin() override;
    void end() override;

    void submit(const CommandBufferSubmitInfo& info) const override;

    void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void begin_render_pass(std::shared_ptr<RenderPass> render_pass) override;
    void end_render_pass() override;

    void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<RayTracingPipeline> pipeline) override;

    void draw(const VertexBuffer& vertex) const override;
    void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const override;

    void draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const override;
    void draw_indexed_instanced(const VertexBuffer& vertex, const IndexBuffer& index, uint32_t instance_count)
        const override;

    void dispatch(glm::uvec3 group_count) const override;

    void trace_rays(glm::uvec3 dimensions) const override;

    void transition_resource(const ImageResource& image, ImageResourceState old_state, ImageResourceState new_state)
        const override;

    void transition_resource(
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewRange range) const override;

    void copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const override;
    void copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const override;

    void build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const override;
    void build_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const override;
    void update_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const override;

    void begin_gpu_marker(const std::string_view& label) const override;
    void end_gpu_marker() const override;

    std::shared_ptr<RenderPass> get_active_render_pass() const override { return m_active_render_pass; }

  private:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    CommandBufferType m_type;

    std::shared_ptr<VulkanRenderPass> m_active_render_pass{nullptr};
    std::shared_ptr<IVulkanPipeline> m_bound_pipeline{nullptr};

    struct ResourceGroupInfo
    {
        std::shared_ptr<VulkanResourceGroup> resource_group;
        uint32_t set;

        bool has_value() const { return resource_group != nullptr; }
        void clear_value() { resource_group = nullptr; }
    };
    std::vector<ResourceGroupInfo> m_bound_resource_groups;

    void clear_bound_resource_groups();
    std::shared_ptr<VulkanQueue> get_queue() const;
};

} // namespace Mizu::Vulkan
