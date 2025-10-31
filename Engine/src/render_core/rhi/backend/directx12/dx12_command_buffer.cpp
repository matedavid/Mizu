#include "dx12_command_buffer.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/render_pass.h"
#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_buffer_resource.h"
#include "render_core/rhi/backend/directx12/dx12_framebuffer.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"
#include "render_core/rhi/backend/directx12/dx12_synchronization.h"

namespace Mizu::Dx12
{

Dx12CommandBuffer::Dx12CommandBuffer(CommandBufferType type) : m_type(type)
{
    m_command_list = Dx12Context.device->allocate_command_list(m_type);
    m_command_allocator = Dx12Context.device->get_command_allocator(m_type);
}

Dx12CommandBuffer::~Dx12CommandBuffer()
{
    Dx12Context.device->free_command_list(m_command_list, m_type);
}

void Dx12CommandBuffer::begin()
{
    DX12_CHECK(m_command_allocator->Reset());
    DX12_CHECK(m_command_list->Reset(m_command_allocator, nullptr));
}

void Dx12CommandBuffer::end()
{
    DX12_CHECK(m_command_list->Close());
}

void Dx12CommandBuffer::submit(const CommandBufferSubmitInfo& info) const
{
    ID3D12CommandList* command_lists[] = {m_command_list};
    get_queue()->ExecuteCommandLists(_countof(command_lists), command_lists);

    if (info.signal_fence != nullptr)
    {
        Dx12Fence& native_fence = dynamic_cast<Dx12Fence&>(*info.signal_fence);
        native_fence.signal(get_queue());
    }
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
    const Dx12Framebuffer& native_framebuffer = dynamic_cast<const Dx12Framebuffer&>(*render_pass->get_framebuffer());

    std::span<const D3D12_RENDER_PASS_RENDER_TARGET_DESC> color_attachment_descriptions =
        native_framebuffer.get_color_attachment_descriptions();

    std::optional<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> depth_stencil_attachment_description_opt =
        native_framebuffer.get_depth_stencil_attachment_description();

    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC* depth_stencil_attachment_description = nullptr;
    if (depth_stencil_attachment_description_opt.has_value())
    {
        depth_stencil_attachment_description = &depth_stencil_attachment_description_opt.value();
    }

    m_command_list->BeginRenderPass(
        static_cast<uint32_t>(color_attachment_descriptions.size()),
        color_attachment_descriptions.data(),
        depth_stencil_attachment_description,
        D3D12_RENDER_PASS_FLAG_NONE);
}

void Dx12CommandBuffer::end_render_pass()
{
    m_command_list->EndRenderPass();
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
    [[maybe_unused]] ImageResourceViewRange range) const
{
    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const Dx12ImageResource& native_image = dynamic_cast<const Dx12ImageResource&>(image);

    const D3D12_RESOURCE_STATES native_old_state = Dx12ImageResource::get_dx12_image_resource_state(old_state);
    const D3D12_RESOURCE_STATES native_new_state = Dx12ImageResource::get_dx12_image_resource_state(new_state);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = native_image.handle();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = native_old_state;
    barrier.Transition.StateAfter = native_new_state;

    m_command_list->ResourceBarrier(1, &barrier);
}

void Dx12CommandBuffer::transition_resource(
    const BufferResource& buffer,
    BufferResourceState old_state,
    BufferResourceState new_state) const
{
    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const Dx12BufferResource& native_buffer = dynamic_cast<const Dx12BufferResource&>(buffer);

    const D3D12_RESOURCE_STATES native_old_state = Dx12BufferResource::get_dx12_buffer_resource_state(old_state);
    const D3D12_RESOURCE_STATES native_new_state = Dx12BufferResource::get_dx12_buffer_resource_state(new_state);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = native_buffer.handle();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = native_old_state;
    barrier.Transition.StateAfter = native_new_state;

    m_command_list->ResourceBarrier(1, &barrier);
}

void Dx12CommandBuffer::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    MIZU_ASSERT(
        source.get_size() == dest.get_size(),
        "Size of buffers do not match ({} != {})",
        source.get_size(),
        dest.get_size());

    const Dx12BufferResource& native_source = dynamic_cast<const Dx12BufferResource&>(source);
    const Dx12BufferResource& native_dest = dynamic_cast<const Dx12BufferResource&>(dest);

    m_command_list->CopyBufferRegion(native_dest.handle(), 0, native_source.handle(), 0, source.get_size());
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

ID3D12CommandQueue* Dx12CommandBuffer::get_queue() const
{
    switch (m_type)
    {
    case CommandBufferType::Graphics:
        return Dx12Context.device->get_graphics_queue();
    case CommandBufferType::Compute:
        return Dx12Context.device->get_compute_queue();
    case CommandBufferType::Transfer:
        return Dx12Context.device->get_transfer_queue();
    }
}

} // namespace Mizu::Dx12