#pragma once

#include "render_core/rhi/command_buffer.h"

#include "dx12_core.h"
#include "dx12_framebuffer.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12Pipeline;

class Dx12CommandBuffer : public CommandBuffer
{
  public:
    Dx12CommandBuffer(CommandBufferType type);
    ~Dx12CommandBuffer() override;

    void begin() override;
    void end() override;

    void submit(const CommandBufferSubmitInfo& info) const override;

    void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) override;
    void push_constant(uint32_t size, const void* data) const override;

    void begin_render_pass(std::shared_ptr<Framebuffer> framebuffer) override;
    void end_render_pass() override;

    void bind_pipeline(std::shared_ptr<Pipeline> pipeline) override;

    void draw(const BufferResource& vertex) const override;
    void draw_indexed(const BufferResource& vertex, const BufferResource& index) const override;

    void draw_instanced(const BufferResource& vertex, uint32_t instance_count) const override;
    void draw_indexed_instanced(const BufferResource& vertex, const BufferResource& index, uint32_t instance_count)
        const override;

    void dispatch(glm::uvec3 group_count) const override;

    void trace_rays(glm::uvec3 dimensions) const override;

    void transition_resource(const ImageResource& image, ImageResourceState old_state, ImageResourceState new_state)
        const override;

    void transition_resource(
        const ImageResource& image,
        ImageResourceState old_state,
        ImageResourceState new_state,
        ImageResourceViewDescription range) const override;

    void transition_resource(const BufferResource& buffer, BufferResourceState old_state, BufferResourceState new_state)
        const override;

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

    std::shared_ptr<Framebuffer> get_active_framebuffer() const override { return m_bound_render_pass; }

  private:
    ID3D12GraphicsCommandList7* m_command_list;
    ID3D12CommandAllocator* m_command_allocator;
    CommandBufferType m_type;

    std::shared_ptr<Dx12Framebuffer> m_bound_render_pass = nullptr;
    std::shared_ptr<Dx12Pipeline> m_bound_pipeline = nullptr;

    ID3D12CommandQueue* get_queue() const;
};

} // namespace Mizu::Dx12