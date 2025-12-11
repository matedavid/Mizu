#include "dx12_command_buffer.h"

#include <map>

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/backend/directx12/dx12_buffer_resource.h"
#include "render_core/rhi/backend/directx12/dx12_framebuffer.h"
#include "render_core/rhi/backend/directx12/dx12_image_resource.h"
#include "render_core/rhi/backend/directx12/dx12_pipeline.h"
#include "render_core/rhi/backend/directx12/dx12_resource_group.h"
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

    ID3D12DescriptorHeap* heaps[] = {
        Dx12Context.heaps.cbv_srv_uav_shader_heap->handle(),
        Dx12Context.heaps.sampler_shader_heap->handle(),
    };
    m_command_list->SetDescriptorHeaps(2, heaps);
}

void Dx12CommandBuffer::end()
{
    m_bound_pipeline = nullptr;
    m_bound_render_pass = nullptr;

    DX12_CHECK(m_command_list->Close());
}

void Dx12CommandBuffer::submit(const CommandBufferSubmitInfo& info) const
{
    for (const std::shared_ptr<Semaphore>& semaphore : info.wait_semaphores)
    {
        Dx12Semaphore& native_semaphore = dynamic_cast<Dx12Semaphore&>(*semaphore);
        native_semaphore.wait(get_queue());
    }

    ID3D12CommandList* command_lists[] = {m_command_list};
    get_queue()->ExecuteCommandLists(_countof(command_lists), command_lists);

    if (info.signal_fence != nullptr)
    {
        Dx12Fence& native_fence = dynamic_cast<Dx12Fence&>(*info.signal_fence);
        native_fence.signal(get_queue());
    }

    for (const std::shared_ptr<Semaphore>& semaphore : info.signal_semaphores)
    {
        Dx12Semaphore& native_semaphore = dynamic_cast<Dx12Semaphore&>(*semaphore);
        native_semaphore.signal(get_queue());
    }
}

void Dx12CommandBuffer::bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set)
{
    (void)set;

    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't bind resource group because no pipeline has been bound");

    const Dx12ResourceGroup& native_resource_group = dynamic_cast<const Dx12ResourceGroup&>(*resource_group);
    native_resource_group.bind_descriptor_table(
        m_command_list,
        *Dx12Context.heaps.cbv_srv_uav_shader_heap,
        *Dx12Context.heaps.sampler_shader_heap,
        m_bound_pipeline->get_pipeline_type());
}

void Dx12CommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    const Dx12RootSignatureInfo& root_signature_info = m_bound_pipeline->get_root_signature_info();
    const uint32_t num_32bit_values = (size + 3) / 4;

    switch (m_bound_pipeline->get_pipeline_type())
    {
    case PipelineType::Graphics:
        m_command_list->SetGraphicsRoot32BitConstants(
            root_signature_info.root_constant_index, num_32bit_values, data, 0);
        break;
    case PipelineType::Compute:
        m_command_list->SetComputeRoot32BitConstants(
            root_signature_info.root_constant_index, num_32bit_values, data, 0);
        break;
    case PipelineType::RayTracing:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }
}

void Dx12CommandBuffer::begin_render_pass(std::shared_ptr<Framebuffer> framebuffer)
{
    MIZU_ASSERT(m_bound_render_pass == nullptr, "Can't begin render pass when another render pass is currently bound");

    m_bound_render_pass = std::static_pointer_cast<Dx12Framebuffer>(framebuffer);

    std::span<const D3D12_RENDER_PASS_RENDER_TARGET_DESC> color_attachment_descriptions =
        m_bound_render_pass->get_color_attachment_descriptions();
    std::optional<D3D12_RENDER_PASS_DEPTH_STENCIL_DESC> depth_stencil_attachment_description_opt =
        m_bound_render_pass->get_depth_stencil_attachment_description();

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

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(m_bound_render_pass->get_width());
    viewport.Height = static_cast<float>(m_bound_render_pass->get_height());
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    D3D12_RECT scissor{};
    scissor.left = static_cast<LONG>(0.0);
    scissor.top = static_cast<LONG>(0.0);
    scissor.right = static_cast<LONG>(m_bound_render_pass->get_width());
    scissor.bottom = static_cast<LONG>(m_bound_render_pass->get_height());

    m_command_list->RSSetViewports(1, &viewport);
    m_command_list->RSSetScissorRects(1, &scissor);
}

void Dx12CommandBuffer::end_render_pass()
{
    m_command_list->EndRenderPass();

    m_bound_render_pass = nullptr;
    m_bound_pipeline = nullptr;
}

void Dx12CommandBuffer::bind_pipeline(std::shared_ptr<Pipeline> pipeline)
{
#if MIZU_DEBUG
    if (m_bound_render_pass != nullptr)
    {
        MIZU_ASSERT(
            pipeline->get_pipeline_type() == PipelineType::Graphics,
            "Can't bind non graphics pipeline if a render pass is active");
    }
    else
    {
        MIZU_ASSERT(
            pipeline->get_pipeline_type() != PipelineType::Graphics,
            "Can't bind graphics pipeline because no render pass is active");
    }
#endif

    m_bound_pipeline = std::static_pointer_cast<Dx12Pipeline>(pipeline);

    switch (m_bound_pipeline->get_pipeline_type())
    {
    case PipelineType::Graphics:
        m_command_list->SetGraphicsRootSignature(m_bound_pipeline->get_root_signature());
        break;
    case PipelineType::Compute:
        m_command_list->SetComputeRootSignature(m_bound_pipeline->get_root_signature());
        break;
    case PipelineType::RayTracing:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }

    m_command_list->SetPipelineState(m_bound_pipeline->handle());

    // TODO: Should probably depend on the topology of the GraphicsPipeline, not sure if I have that option in the
    // GraphicsPipeline creation.
    m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
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
    const Dx12BufferResource& native_vertex_resource = dynamic_cast<const Dx12BufferResource&>(*vertex.get_resource());

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
    vertex_buffer_view.BufferLocation = native_vertex_resource.get_gpu_address();
    vertex_buffer_view.SizeInBytes = static_cast<uint32_t>(vertex.get_size());
    vertex_buffer_view.StrideInBytes = vertex.get_stride();

    m_command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

    m_command_list->DrawInstanced(vertex.get_count(), instance_count, 0, 0);
}

void Dx12CommandBuffer::draw_indexed_instanced(
    const VertexBuffer& vertex,
    const IndexBuffer& index,
    uint32_t instance_count) const
{
    const Dx12BufferResource& native_vertex_resource = dynamic_cast<const Dx12BufferResource&>(*vertex.get_resource());
    const Dx12BufferResource& native_index_resource = dynamic_cast<const Dx12BufferResource&>(*index.get_resource());

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
    vertex_buffer_view.BufferLocation = native_vertex_resource.get_gpu_address();
    vertex_buffer_view.SizeInBytes = static_cast<uint32_t>(vertex.get_size());
    vertex_buffer_view.StrideInBytes = vertex.get_stride();

    D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
    index_buffer_view.BufferLocation = native_index_resource.get_gpu_address();
    index_buffer_view.SizeInBytes = static_cast<uint32_t>(index.get_count() * sizeof(uint32_t));
    index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

    m_command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    m_command_list->IASetIndexBuffer(&index_buffer_view);

    m_command_list->DrawIndexedInstanced(index.get_count(), instance_count, 0, 0, 0);
}

void Dx12CommandBuffer::dispatch(glm::uvec3 group_count) const
{
    // TODO: Also check that is a compute pipeline
    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't draw_indexed_instance because no compute pipeline has been bound");

    m_command_list->Dispatch(group_count.x, group_count.y, group_count.z);
}

void Dx12CommandBuffer::trace_rays(glm::uvec3 dimensions) const
{
    (void)dimensions;
    MIZU_UNREACHABLE("Not implemented");
}

struct TransitionInfo
{
    D3D12_BARRIER_SYNC sync_before, sync_after;
    D3D12_BARRIER_ACCESS access_before, access_after;
};

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
    if (old_state == new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const Dx12ImageResource& native_image = dynamic_cast<const Dx12ImageResource&>(image);

    const D3D12_BARRIER_LAYOUT native_old_state =
        Dx12ImageResource::get_dx12_image_barrier_layout(old_state, native_image.get_format());
    const D3D12_BARRIER_LAYOUT native_new_state =
        Dx12ImageResource::get_dx12_image_barrier_layout(new_state, native_image.get_format());

#define DEFINE_TRANSITION(oldl, newl, sync_before, sync_after, access_before, access_after) \
    {                                                                                       \
        {ImageResourceState::oldl, ImageResourceState::newl}, TransitionInfo                \
        {                                                                                   \
            sync_before, sync_after, access_before, access_after                            \
        }                                                                                   \
    }

    // NOTE: At the moment only specifying "expected transitions"
    static std::map<std::pair<ImageResourceState, ImageResourceState>, TransitionInfo> s_transition_info{
        // Undefined
        DEFINE_TRANSITION(
            Undefined,
            UnorderedAccess,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS),

        DEFINE_TRANSITION(
            Undefined,
            TransferDst,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST),

        DEFINE_TRANSITION(
            Undefined,
            ColorAttachment,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_RENDER_TARGET),

        DEFINE_TRANSITION(
            Undefined,
            DepthStencilAttachment,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE),

        // UnorderedAccess
        DEFINE_TRANSITION(
            UnorderedAccess,
            ShaderReadOnly,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE),

        DEFINE_TRANSITION(
            UnorderedAccess,
            Present,
            D3D12_BARRIER_SYNC_ALL,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            D3D12_BARRIER_ACCESS_NO_ACCESS),

        // TransferDst
        DEFINE_TRANSITION(
            TransferDst,
            ShaderReadOnly,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE),

        // ShaderReadOnly
        DEFINE_TRANSITION(
            ShaderReadOnly,
            UnorderedAccess,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS),

        DEFINE_TRANSITION(
            ShaderReadOnly,
            DepthStencilAttachment,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ),

        DEFINE_TRANSITION(
            ShaderReadOnly,
            Present,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_ACCESS_COMMON),

        // ColorAttachment
        DEFINE_TRANSITION(
            ColorAttachment,
            ShaderReadOnly,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE),

        DEFINE_TRANSITION(
            ColorAttachment,
            Present,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_ACCESS_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_NO_ACCESS),

        // DepthStencilAttachment
        DEFINE_TRANSITION(
            DepthStencilAttachment,
            ShaderReadOnly,
            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE),
    };

#undef DEFINE_TRANSITION

    const auto it = s_transition_info.find({old_state, new_state});
    if (it == s_transition_info.end())
    {
        MIZU_UNREACHABLE(
            "Image layout transition not defined: {} -> {} for texture: {}",
            image_resource_state_to_string(old_state),
            image_resource_state_to_string(new_state),
            native_image.get_name());
        return;
    }

    const TransitionInfo& info = it->second;

    D3D12_BARRIER_SUBRESOURCE_RANGE subresource_range{};
    subresource_range.IndexOrFirstMipLevel = range.get_mip_base();
    subresource_range.NumMipLevels = range.get_mip_count();
    subresource_range.FirstArraySlice = range.get_layer_base();
    subresource_range.NumArraySlices = range.get_layer_count();
    subresource_range.FirstPlane = 0;
    subresource_range.NumPlanes = 1;

    D3D12_TEXTURE_BARRIER texture_barrier{};
    texture_barrier.LayoutBefore = native_old_state;
    texture_barrier.LayoutAfter = native_new_state;
    texture_barrier.SyncBefore = info.sync_before;
    texture_barrier.SyncAfter = info.sync_after;
    texture_barrier.AccessBefore = info.access_before;
    texture_barrier.AccessAfter = info.access_after;
    texture_barrier.pResource = native_image.handle();
    texture_barrier.Subresources = subresource_range;
    texture_barrier.Flags = D3D12_TEXTURE_BARRIER_FLAG_NONE;

    D3D12_BARRIER_GROUP barrier_group{};
    barrier_group.Type = D3D12_BARRIER_TYPE_TEXTURE;
    barrier_group.NumBarriers = 1;
    barrier_group.pTextureBarriers = &texture_barrier;

    m_command_list->Barrier(1, &barrier_group);
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

#define DEFINE_TRANSITION(oldl, newl, sync_before, sync_after, access_before, access_after) \
    {                                                                                       \
        {BufferResourceState::oldl, BufferResourceState::newl}, TransitionInfo              \
        {                                                                                   \
            sync_before, sync_after, access_before, access_after                            \
        }                                                                                   \
    }

    // NOTE: At the moment only specifying "expected transitions"
    static std::map<std::pair<BufferResourceState, BufferResourceState>, TransitionInfo> s_transition_info{
        // Undefined
        DEFINE_TRANSITION(
            Undefined,
            TransferDst,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST),

        DEFINE_TRANSITION(
            Undefined,
            TransferSrc,
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST),

        // TransferDst
        DEFINE_TRANSITION(
            TransferDst,
            ShaderReadOnly,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE),
    };

#undef DEFINE_TRANSITION

    const auto it = s_transition_info.find({old_state, new_state});
    if (it == s_transition_info.end())
    {
        MIZU_UNREACHABLE(
            "Buffer layout transition not defined: {} -> {} for buffer: {}",
            buffer_resource_state_to_string(old_state),
            buffer_resource_state_to_string(new_state),
            native_buffer.get_name());
        return;
    }

    const TransitionInfo& info = it->second;

    D3D12_BUFFER_BARRIER buffer_barrier{};
    buffer_barrier.SyncBefore = info.sync_before;
    buffer_barrier.SyncAfter = info.sync_after;
    buffer_barrier.AccessBefore = info.access_before;
    buffer_barrier.AccessAfter = info.access_after;
    buffer_barrier.pResource = native_buffer.handle();
    buffer_barrier.Offset = native_buffer.get_allocation_info().offset;
    buffer_barrier.Size = native_buffer.get_size();

    D3D12_BARRIER_GROUP barrier_group{};
    barrier_group.Type = D3D12_BARRIER_TYPE_BUFFER;
    barrier_group.NumBarriers = 1;
    barrier_group.pBufferBarriers = &buffer_barrier;

    m_command_list->Barrier(1, &barrier_group);
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
    const Dx12ImageResource& native_image = dynamic_cast<const Dx12ImageResource&>(image);
    const Dx12BufferResource& native_buffer = dynamic_cast<const Dx12BufferResource&>(buffer);

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT image_footprint{};
    native_image.get_copyable_footprints(&image_footprint, nullptr, nullptr, nullptr);

    D3D12_TEXTURE_COPY_LOCATION dest_copy_location{};
    dest_copy_location.pResource = native_image.handle();
    dest_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dest_copy_location.SubresourceIndex = 0;

    D3D12_TEXTURE_COPY_LOCATION src_copy_location{};
    src_copy_location.pResource = native_buffer.handle();
    src_copy_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    src_copy_location.PlacedFootprint = image_footprint;

    m_command_list->CopyTextureRegion(&dest_copy_location, 0, 0, 0, &src_copy_location, nullptr);
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
    // MIZU_UNREACHABLE("Not implemented");
}

void Dx12CommandBuffer::end_gpu_marker() const
{
    // MIZU_UNREACHABLE("Not implemented");
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