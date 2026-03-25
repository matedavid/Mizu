#include "dx12_command_buffer.h"

#include <map>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "dx12_buffer_resource.h"
#include "dx12_context.h"
#include "dx12_image_resource.h"
#include "dx12_pipeline.h"
#include "dx12_resource_view.h"
#include "dx12_synchronization.h"
#include "dx12_types.h"

namespace Mizu::Dx12
{

Dx12CommandBuffer::Dx12CommandBuffer(CommandBufferType type) : m_type(type)
{
    m_command_list = Dx12Context.device->allocate_command_list(m_type, Dx12Context.current_frame_idx);
}

Dx12CommandBuffer::~Dx12CommandBuffer()
{
    Dx12Context.device->free_command_list(m_command_list, m_type);
}

void Dx12CommandBuffer::begin()
{
    m_command_allocator = Dx12Context.device->get_thread_command_allocator(m_type, Dx12Context.current_frame_idx);

    DX12_CHECK(m_command_list->Reset(m_command_allocator, nullptr));

    Dx12Context.descriptor_manager->set_descriptor_heaps(m_command_list);
}

void Dx12CommandBuffer::end()
{
    m_bound_pipeline = nullptr;

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

void Dx12CommandBuffer::bind_descriptor_set(std::shared_ptr<DescriptorSet> descriptor_set, uint32_t set)
{
    MIZU_ASSERT(m_bound_pipeline != nullptr, "Can't bind resource group because no pipeline has been bound");

    const Dx12RootSignatureInfo& root_signature_info = m_bound_pipeline->get_root_signature_info();

    const Dx12DescriptorSet& native_descriptor_set = static_cast<const Dx12DescriptorSet&>(*descriptor_set);

    MIZU_ASSERT(set < MAX_DESCRIPTOR_SET_COUNT, "Descriptor set index {} exceeds MAX_DESCRIPTOR_SET_COUNT", set);

    const uint32_t resource_param_idx = root_signature_info.resource_root_param_index[set];
    if (resource_param_idx != Dx12RootSignatureInfo::INVALID_ROOT_PARAM_INDEX)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE resource_gpu_handle = native_descriptor_set.get_resource_gpu_handle();

        switch (m_bound_pipeline->get_pipeline_type())
        {
        case PipelineType::Graphics:
            m_command_list->SetGraphicsRootDescriptorTable(resource_param_idx, resource_gpu_handle);
            break;
        case PipelineType::Compute:
            m_command_list->SetComputeRootDescriptorTable(resource_param_idx, resource_gpu_handle);
            break;
        case PipelineType::RayTracing:
            MIZU_UNREACHABLE("Not implemented");
            break;
        }
    }

    if (native_descriptor_set.get_sampler_allocation().count > 0)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu_handle = native_descriptor_set.get_sampler_gpu_handle();

        const uint32_t sampler_param_idx = root_signature_info.sampler_root_param_index[set];
        MIZU_ASSERT(
            sampler_param_idx != Dx12RootSignatureInfo::INVALID_ROOT_PARAM_INDEX,
            "No sampler root parameter found for descriptor set {}",
            set);

        switch (m_bound_pipeline->get_pipeline_type())
        {
        case PipelineType::Graphics:
            m_command_list->SetGraphicsRootDescriptorTable(sampler_param_idx, sampler_gpu_handle);
            break;
        case PipelineType::Compute:
            m_command_list->SetComputeRootDescriptorTable(sampler_param_idx, sampler_gpu_handle);
            break;
        case PipelineType::RayTracing:
            MIZU_UNREACHABLE("Not implemented");
            break;
        }
    }
}

void Dx12CommandBuffer::push_constant(uint32_t size, const void* data) const
{
    const Dx12RootSignatureInfo& root_signature_info = m_bound_pipeline->get_root_signature_info();
    const uint32_t num_32bit_values = (size + 3) / 4;

    switch (m_bound_pipeline->get_pipeline_type())
    {
    case PipelineType::Graphics:
        m_command_list->SetGraphicsRoot32BitConstants(
            root_signature_info.root_constant_offset, num_32bit_values, data, 0);
        break;
    case PipelineType::Compute:
        m_command_list->SetComputeRoot32BitConstants(
            root_signature_info.root_constant_offset, num_32bit_values, data, 0);
        break;
    case PipelineType::RayTracing:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }
}

void Dx12CommandBuffer::begin_render_pass(const RenderPassInfo& info)
{
    MIZU_ASSERT(!m_render_pass_active, "Can't begin render pass when another render pass is currently bound");

    inplace_vector<D3D12_RENDER_PASS_RENDER_TARGET_DESC, MAX_FRAMEBUFFER_COLOR_ATTACHMENTS> color_attachments;
    D3D12_RENDER_PASS_DEPTH_STENCIL_DESC depth_stencil_attachment;

    for (const FramebufferAttachment& attachment : info.color_attachments)
    {
        const ImageResourceView& rtv = attachment.rtv;
        MIZU_ASSERT(rtv.image != nullptr, "Invalid image in rtv");

        Dx12ImageResource& native_rtv_image = static_cast<Dx12ImageResource&>(*rtv.image);

        const Dx12ImageResourceView internal_rtv = native_rtv_image.as_rtv(rtv.desc);
        MIZU_ASSERT(!is_depth_format(internal_rtv.format), "Can't use a rtv with a depth format as a color attachment");

        D3D12_RENDER_PASS_BEGINNING_ACCESS beginning_access{};
        beginning_access.Type = get_dx12_load_operation(attachment.load_operation);

        if (attachment.load_operation == LoadOperation::Clear)
        {
            beginning_access.Clear.ClearValue.Format = get_dx12_image_format(internal_rtv.format);
            beginning_access.Clear.ClearValue.Color[0] = attachment.clear_value.r;
            beginning_access.Clear.ClearValue.Color[1] = attachment.clear_value.g;
            beginning_access.Clear.ClearValue.Color[2] = attachment.clear_value.b;
            beginning_access.Clear.ClearValue.Color[3] = attachment.clear_value.a;
        }

        D3D12_RENDER_PASS_ENDING_ACCESS ending_access{};
        ending_access.Type = get_dx12_store_operation(attachment.store_operation);

        D3D12_RENDER_PASS_RENDER_TARGET_DESC color_attachment_desc{};
        color_attachment_desc.cpuDescriptor = internal_rtv.handle;
        color_attachment_desc.BeginningAccess = beginning_access;
        color_attachment_desc.EndingAccess = ending_access;
        color_attachments.push_back(color_attachment_desc);
    }

    if (info.depth_stencil_attachment.has_value())
    {
        const FramebufferAttachment& attachment = *info.depth_stencil_attachment;

        const ImageResourceView& rtv = attachment.rtv;
        MIZU_ASSERT(rtv.image != nullptr, "Invalid image in rtv");

        Dx12ImageResource& native_rtv_image = static_cast<Dx12ImageResource&>(*rtv.image);

        const Dx12ImageResourceView internal_rtv = native_rtv_image.as_rtv(rtv.desc);
        MIZU_ASSERT(
            is_depth_format(internal_rtv.format), "Can't use a rtv with a non depth format as a depth attachment");

        depth_stencil_attachment.cpuDescriptor = internal_rtv.handle;

        D3D12_RENDER_PASS_BEGINNING_ACCESS depth_beginning_access{};
        depth_beginning_access.Type = get_dx12_load_operation(attachment.load_operation);

        if (attachment.load_operation == LoadOperation::Clear)
        {
            depth_beginning_access.Clear.ClearValue.Format = get_dx12_image_format(internal_rtv.format);
            depth_beginning_access.Clear.ClearValue.DepthStencil.Depth = attachment.clear_value.r;
        }

        D3D12_RENDER_PASS_ENDING_ACCESS depth_ending_access{};
        depth_ending_access.Type = get_dx12_store_operation(attachment.store_operation);

        depth_stencil_attachment.DepthBeginningAccess = depth_beginning_access;
        depth_stencil_attachment.DepthEndingAccess = depth_ending_access;
        // TODO: stencils
        depth_stencil_attachment.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
        depth_stencil_attachment.StencilEndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
    }

    m_command_list->BeginRenderPass(
        static_cast<uint32_t>(color_attachments.size()),
        color_attachments.data(),
        info.depth_stencil_attachment.has_value() ? &depth_stencil_attachment : nullptr,
        D3D12_RENDER_PASS_FLAG_NONE);

    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = static_cast<float>(info.offset.x);
    viewport.TopLeftY = static_cast<float>(info.offset.y);
    viewport.Width = static_cast<float>(info.extent.x);
    viewport.Height = static_cast<float>(info.extent.y);
    viewport.MinDepth = info.min_depth;
    viewport.MaxDepth = info.max_depth;

    D3D12_RECT scissor{};
    scissor.left = static_cast<LONG>(info.offset.x);
    scissor.top = static_cast<LONG>(info.offset.y);
    scissor.right = static_cast<LONG>(info.extent.x);
    scissor.bottom = static_cast<LONG>(info.extent.y);

    m_command_list->RSSetViewports(1, &viewport);
    m_command_list->RSSetScissorRects(1, &scissor);

    m_render_pass_active = true;
}

void Dx12CommandBuffer::end_render_pass()
{
    m_command_list->EndRenderPass();

    m_render_pass_active = false;
    m_bound_pipeline = nullptr;
}

void Dx12CommandBuffer::bind_pipeline(std::shared_ptr<Pipeline> pipeline)
{
#if MIZU_DEBUG
    if (m_render_pass_active)
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
        // TODO: Should probably depend on the topology of the GraphicsPipeline, not sure if I have that option in the
        // GraphicsPipeline creation.
        m_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        break;
    case PipelineType::Compute:
        m_command_list->SetComputeRootSignature(m_bound_pipeline->get_root_signature());
        break;
    case PipelineType::RayTracing:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }

    m_command_list->SetPipelineState(m_bound_pipeline->handle());
}

void Dx12CommandBuffer::draw(const BufferResource& vertex) const
{
    draw_instanced(vertex, 1);
}

void Dx12CommandBuffer::draw_indexed(const BufferResource& vertex, const BufferResource& index) const
{
    draw_indexed_instanced(vertex, index, 1);
}

void Dx12CommandBuffer::draw_instanced(const BufferResource& vertex, uint32_t instance_count) const
{
    const Dx12BufferResource& native_vertex_resource = static_cast<const Dx12BufferResource&>(vertex);

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
    vertex_buffer_view.BufferLocation = native_vertex_resource.get_gpu_address();
    vertex_buffer_view.SizeInBytes = static_cast<uint32_t>(vertex.get_size());
    vertex_buffer_view.StrideInBytes = static_cast<uint32_t>(vertex.get_stride());

    m_command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);

    const uint32_t vertex_count = static_cast<uint32_t>(vertex.get_size() / vertex.get_stride());
    m_command_list->DrawInstanced(vertex_count, instance_count, 0, 0);
}

void Dx12CommandBuffer::draw_indexed_instanced(
    const BufferResource& vertex,
    const BufferResource& index,
    uint32_t instance_count) const
{
    const Dx12BufferResource& native_vertex_resource = static_cast<const Dx12BufferResource&>(vertex);
    const Dx12BufferResource& native_index_resource = static_cast<const Dx12BufferResource&>(index);

    D3D12_VERTEX_BUFFER_VIEW vertex_buffer_view{};
    vertex_buffer_view.BufferLocation = native_vertex_resource.get_gpu_address();
    vertex_buffer_view.SizeInBytes = static_cast<uint32_t>(vertex.get_size());
    vertex_buffer_view.StrideInBytes = static_cast<uint32_t>(vertex.get_stride());

    D3D12_INDEX_BUFFER_VIEW index_buffer_view{};
    index_buffer_view.BufferLocation = native_index_resource.get_gpu_address();
    index_buffer_view.SizeInBytes = static_cast<uint32_t>(index.get_size());
    index_buffer_view.Format = DXGI_FORMAT_R32_UINT;

    m_command_list->IASetVertexBuffers(0, 1, &vertex_buffer_view);
    m_command_list->IASetIndexBuffer(&index_buffer_view);

    const uint32_t index_count = static_cast<uint32_t>(index.get_size() / sizeof(uint32_t));
    m_command_list->DrawIndexedInstanced(index_count, instance_count, 0, 0, 0);
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

void Dx12CommandBuffer::transition_resource(const BufferResource& buffer, const BufferTransitionInfo& info) const
{
    if (info.old_state == info.new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    // In D3D12, cross-queue ownership transfer is handled by fences, not paired barriers.
    // The release side performs the full layout transition; the acquire side is a no-op.
    if (info.transition_mode == ResourceTransitionMode::Acquire)
        return;

    const Dx12BufferResource& native_buffer = static_cast<const Dx12BufferResource&>(buffer);

    const auto get_dx12_barrier_sync = [&](BufferResourceState state) -> D3D12_BARRIER_SYNC {
        switch (state)
        {
        case BufferResourceState::Undefined:
            return D3D12_BARRIER_SYNC_NONE;
        case BufferResourceState::ShaderReadOnly:
            if (m_type == CommandBufferType::Graphics)
                return D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING;
            else
                return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        case BufferResourceState::UnorderedAccess:
            // In d3d12, it's valid to write into a uav from a pixel shader.
            if (m_type == CommandBufferType::Graphics)
                return D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING;
            else
                return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        case BufferResourceState::TransferSrc:
            return D3D12_BARRIER_SYNC_COPY;
        case BufferResourceState::TransferDst:
            return D3D12_BARRIER_SYNC_COPY;
        case BufferResourceState::AccelStructScratch:
        case BufferResourceState::AccelStructBuildInput:
            MIZU_UNREACHABLE("Not implemented");
            return D3D12_BARRIER_SYNC_NONE;
        }
    };

    const auto get_dx12_barrier_access = [&](BufferResourceState state) -> D3D12_BARRIER_ACCESS {
        switch (state)
        {
        case BufferResourceState::Undefined:
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        case BufferResourceState::ShaderReadOnly: {
            D3D12_BARRIER_ACCESS shader_resource_access = D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
            if (native_buffer.get_usage() & BufferUsageBits::ConstantBuffer)
                shader_resource_access |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
            if (native_buffer.get_usage() & BufferUsageBits::VertexBuffer)
                shader_resource_access |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
            if (native_buffer.get_usage() & BufferUsageBits::IndexBuffer)
                shader_resource_access |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;

            return shader_resource_access;
        }
        case BufferResourceState::UnorderedAccess:
            return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        case BufferResourceState::TransferSrc:
            return D3D12_BARRIER_ACCESS_COPY_SOURCE;
        case BufferResourceState::TransferDst:
            return D3D12_BARRIER_ACCESS_COPY_DEST;
        case BufferResourceState::AccelStructScratch:
        case BufferResourceState::AccelStructBuildInput:
            MIZU_UNREACHABLE("Not implemented");
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        }
    };

    const D3D12_BARRIER_SYNC sync_before = get_dx12_barrier_sync(info.old_state);
    const D3D12_BARRIER_SYNC sync_after = get_dx12_barrier_sync(info.new_state);

    const D3D12_BARRIER_ACCESS access_before = get_dx12_barrier_access(info.old_state);
    const D3D12_BARRIER_ACCESS access_after = get_dx12_barrier_access(info.new_state);

    D3D12_BUFFER_BARRIER buffer_barrier{};
    buffer_barrier.SyncBefore = sync_before;
    buffer_barrier.SyncAfter = sync_after;
    buffer_barrier.AccessBefore = access_before;
    buffer_barrier.AccessAfter = access_after;
    buffer_barrier.pResource = native_buffer.handle();
    buffer_barrier.Offset = 0;
    buffer_barrier.Size = UINT64_MAX;

    D3D12_BARRIER_GROUP barrier_group{};
    barrier_group.Type = D3D12_BARRIER_TYPE_BUFFER;
    barrier_group.NumBarriers = 1;
    barrier_group.pBufferBarriers = &buffer_barrier;

    m_command_list->Barrier(1, &barrier_group);
}

void Dx12CommandBuffer::transition_resource(const ImageResource& image, const ImageTransitionInfo& info) const
{
    if (info.old_state == info.new_state)
    {
        MIZU_LOG_WARNING("Old state and New state are the same");
        return;
    }

    const Dx12ImageResource& native_image = static_cast<const Dx12ImageResource&>(image);

    const auto get_dx12_barrier_sync = [&](ImageResourceState state) -> D3D12_BARRIER_SYNC {
        switch (state)
        {
        case ImageResourceState::Undefined:
            return D3D12_BARRIER_SYNC_NONE;
        case ImageResourceState::ShaderReadOnly:
            if (m_type == CommandBufferType::Graphics)
                return D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING;
            else
                return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        case ImageResourceState::UnorderedAccess:
            // In d3d12, it's valid to write into a uav from a pixel shader.
            if (m_type == CommandBufferType::Graphics)
                return D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING;
            else
                return D3D12_BARRIER_SYNC_COMPUTE_SHADING;
        case ImageResourceState::TransferSrc:
            return D3D12_BARRIER_SYNC_COPY;
        case ImageResourceState::TransferDst:
            return D3D12_BARRIER_SYNC_COPY;
        case ImageResourceState::ColorAttachment:
            return D3D12_BARRIER_SYNC_RENDER_TARGET;
        case ImageResourceState::DepthStencilAttachment:
            return D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        case ImageResourceState::Present:
            return D3D12_BARRIER_SYNC_NONE;
        }
    };

    const auto get_dx12_barrier_access = [&](ImageResourceState state) -> D3D12_BARRIER_ACCESS {
        switch (state)
        {
        case ImageResourceState::Undefined:
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        case ImageResourceState::ShaderReadOnly:
            return D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
        case ImageResourceState::UnorderedAccess:
            return D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
        case ImageResourceState::TransferSrc:
            return D3D12_BARRIER_ACCESS_COPY_SOURCE;
        case ImageResourceState::TransferDst:
            return D3D12_BARRIER_ACCESS_COPY_DEST;
        case ImageResourceState::ColorAttachment:
            return D3D12_BARRIER_ACCESS_RENDER_TARGET;
        case ImageResourceState::DepthStencilAttachment:
            return D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
        case ImageResourceState::Present:
            return D3D12_BARRIER_ACCESS_NO_ACCESS;
        }
    };

    D3D12_BARRIER_LAYOUT layout_before = get_dx12_image_barrier_layout(info.old_state);
    D3D12_BARRIER_LAYOUT layout_after = get_dx12_image_barrier_layout(info.new_state);

    D3D12_BARRIER_SYNC sync_before = get_dx12_barrier_sync(info.old_state);
    D3D12_BARRIER_SYNC sync_after = get_dx12_barrier_sync(info.new_state);

    D3D12_BARRIER_ACCESS access_before = get_dx12_barrier_access(info.old_state);
    D3D12_BARRIER_ACCESS access_after = get_dx12_barrier_access(info.new_state);

    if (info.transition_mode == ResourceTransitionMode::Release)
    {
        layout_after = D3D12_BARRIER_LAYOUT_COMMON;
        sync_after = D3D12_BARRIER_SYNC_NONE;
        access_after = D3D12_BARRIER_ACCESS_NO_ACCESS;
    }
    else if (info.transition_mode == ResourceTransitionMode::Acquire)
    {
        layout_before = D3D12_BARRIER_LAYOUT_COMMON;
        sync_before = D3D12_BARRIER_SYNC_NONE;
        access_before = D3D12_BARRIER_ACCESS_NO_ACCESS;
    }

    D3D12_BARRIER_SUBRESOURCE_RANGE subresource_range{};
    subresource_range.IndexOrFirstMipLevel = info.view_desc.mip_base;
    subresource_range.NumMipLevels = info.view_desc.mip_count;
    subresource_range.FirstArraySlice = info.view_desc.layer_base;
    subresource_range.NumArraySlices = info.view_desc.layer_count;
    subresource_range.FirstPlane = 0;
    subresource_range.NumPlanes = 1;

    D3D12_TEXTURE_BARRIER texture_barrier{};
    texture_barrier.LayoutBefore = layout_before;
    texture_barrier.LayoutAfter = layout_after;
    texture_barrier.SyncBefore = sync_before;
    texture_barrier.SyncAfter = sync_after;
    texture_barrier.AccessBefore = access_before;
    texture_barrier.AccessAfter = access_after;
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
    const AccelerationStructure& accel_struct,
    const AccelerationStructureTransitionInfo& info) const
{
    (void)accel_struct;
    (void)info;
    MIZU_UNREACHABLE("Not implemented");
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

void Dx12CommandBuffer::begin_gpu_marker(std::string_view label) const
{
    DX12_DEBUG_BEGIN_GPU_MARKER(m_command_list, label);
}

void Dx12CommandBuffer::end_gpu_marker() const
{
    DX12_DEBUG_END_GPU_MARKER(m_command_list);
}

ID3D12CommandQueue* Dx12CommandBuffer::get_queue() const
{
    return Dx12Context.device->get_queue(m_type);
}

} // namespace Mizu::Dx12
