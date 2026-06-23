#pragma once

#include <vector>

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/render_pass.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanPipeline;
class VulkanQueue;

class VulkanCommandBuffer : public CommandBuffer
{
  public:
    VulkanCommandBuffer(CommandBufferType type);
    ~VulkanCommandBuffer() override;

    void begin() override;
    void end() override;

    void submit(const CommandBufferSubmitInfo& info) const override;

    void bind_descriptor_set(std::shared_ptr<DescriptorSet> descriptor_set, uint32_t set) override;
    void push_constant(uint32_t size, const void* data) const override;

    void begin_render_pass(const RenderPassInfo& info) override;
    void end_render_pass() override;
    bool is_render_pass_active() const override { return m_render_pass_active; }

    void bind_pipeline(std::shared_ptr<Pipeline> pipeline) override;

    void bind_vertex_buffer(const BufferResource& vertex_buffer, uint64_t offset = 0) override;
    void bind_index_buffer(const BufferResource& index_buffer, IndexBufferFormat format, uint64_t offset = 0) override;

    void draw(uint32_t vertex_count, uint32_t first_vertex, uint32_t instance_count = 1, uint32_t first_instance = 0)
        override;
    void draw_indexed(
        uint32_t index_count,
        uint32_t first_index,
        uint32_t first_vertex,
        uint32_t instance_count = 1,
        uint32_t first_instance = 0) override;

    void draw(const BufferResource& vertex) const override;
    void draw_indexed(const BufferResource& vertex, const BufferResource& index) const override;

    void draw_instanced(const BufferResource& vertex, uint32_t instance_count) const override;
    void draw_indexed_instanced(const BufferResource& vertex, const BufferResource& index, uint32_t instance_count)
        const override;

    void dispatch(glm::uvec3 group_count) const override;

    void trace_rays(glm::uvec3 dimensions) const override;

    void transition_resource(const BufferResource& buffer, const BufferTransitionInfo& info) const override;
    void transition_resource(const ImageResource& image, const ImageTransitionInfo& info) const override;
    void transition_resource(const AccelerationStructure& accel_struct, const AccelerationStructureTransitionInfo& info)
        const override;

    void copy_buffer_to_buffer(
        const BufferResource& source,
        const BufferResource& dest,
        const CopyBufferToBufferInfo& info) const override;
    void copy_buffer_to_image(
        const BufferResource& buffer,
        const ImageResource& image,
        const CopyBufferToImageInfo& info) const override;

    void build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const override;
    void build_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const override;
    void update_tlas(
        const AccelerationStructure& tlas,
        std::span<AccelerationStructureInstanceData> instances,
        const BufferResource& scratch_buffer) const override;

    void begin_gpu_marker(std::string_view label) const override;
    void end_gpu_marker() const override;

  private:
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    CommandBufferType m_type;

    bool m_render_pass_active = false;

    std::shared_ptr<VulkanPipeline> m_bound_pipeline{nullptr};

#if MIZU_VULKAN_VALIDATIONS_ENABLED
    struct DebugBoundVertexBuffer
    {
        bool is_bound = false;
        uint64_t offset = 0;
        uint64_t remaining_size = 0;
        uint32_t stride = 0;
        uint32_t vertex_count = 0;
    };

    struct DebugBoundIndexBuffer
    {
        bool is_bound = false;
        IndexBufferFormat format = IndexBufferFormat::UInt32;
        uint64_t offset = 0;
        uint64_t remaining_size = 0;
        uint32_t index_size = 0;
        uint32_t index_count = 0;
    };

    DebugBoundVertexBuffer m_debug_bound_vertex_buffer{};
    DebugBoundIndexBuffer m_debug_bound_index_buffer{};
#endif

    std::shared_ptr<VulkanQueue> get_queue() const;
};

} // namespace Mizu::Vulkan
