#include "dx12_command_buffer.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

namespace Mizu::Dx12
{

Dx12CommandBuffer::Dx12CommandBuffer(CommandBufferType type) : m_type(type)
{
    m_command_list = Dx12Context.device->allocate_command_list(m_type);
}

Dx12CommandBuffer::~Dx12CommandBuffer()
{
    Dx12Context.device->free_command_list(m_command_list, m_type);
}

void Dx12CommandBuffer::begin()
{
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::end()
{
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::submit(const CommandBufferSubmitInfo& info) const
{
    (void)info;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set)
{
    (void)resource_group;
    (void)set;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    (void)name;
    (void)size;
    (void)data;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::begin_render_pass(std::shared_ptr<RenderPass> render_pass)
{
    (void)render_pass;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::end_render_pass()
{
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    (void)pipeline;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    (void)pipeline;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::bind_pipeline(std::shared_ptr<RayTracingPipeline> pipeline)
{
    (void)pipeline;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::draw(const VertexBuffer& vertex) const
{
    draw_instanced(vertex, 1);
}

void Dx12CommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    draw_indexed_instanced(vertex, index, 1);
}

void Dx12CommandBuffer::draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const
{
    (void)vertex;
    (void)instance_count;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::draw_indexed_instanced(
    const VertexBuffer& vertex,
    const IndexBuffer& index,
    uint32_t instance_count) const
{
    (void)vertex;
    (void)index;
    (void)instance_count;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::dispatch(glm::uvec3 group_count) const
{
    (void)group_count;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::trace_rays(glm::uvec3 dimensions) const
{
    (void)dimensions;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state) const
{
    const ImageResourceViewRange range =
        ImageResourceViewRange::from_mips_layers(0, image.get_num_mips(), 0, image.get_num_layers());

    transition_resource(image, old_state, new_state, range);
}

void Dx12CommandBuffer::transition_resource(
    const ImageResource& image,
    ImageResourceState old_state,
    ImageResourceState new_state,
    ImageResourceViewRange range) const
{
    (void)image;
    (void)old_state;
    (void)new_state;
    (void)range;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    (void)source;
    (void)dest;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const
{
    (void)buffer;
    (void)image;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::build_blas(const AccelerationStructure& blas, const BufferResource& scratch_buffer) const
{
    (void)blas;
    (void)scratch_buffer;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::build_tlas(
    const AccelerationStructure& tlas,
    std::span<AccelerationStructureInstanceData> instances,
    const BufferResource& scratch_buffer) const
{
    (void)tlas;
    (void)instances;
    (void)scratch_buffer;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::update_tlas(
    const AccelerationStructure& tlas,
    std::span<AccelerationStructureInstanceData> instances,
    const BufferResource& scratch_buffer) const
{
    (void)tlas;
    (void)instances;
    (void)scratch_buffer;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::begin_gpu_marker(const std::string_view& label) const
{
    (void)label;
    MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::end_gpu_marker() const
{
    MIZU_UNREACHABLE("Not implemented");
}

} // namespace Mizu::Dx12