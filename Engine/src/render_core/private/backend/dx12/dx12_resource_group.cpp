#include "backend/dx12/dx12_resource_group.h"

#include "backend/dx12/dx12_context.h"
#include "backend/dx12/dx12_descriptors.h"
#include "backend/dx12/dx12_pipeline.h"
#include "backend/dx12/dx12_resource_view.h"
#include "backend/dx12/dx12_sampler_state.h"

namespace Mizu::Dx12
{

#define BUILDER_ITEM_TYPE_CASE(_type, _vector) \
    else if (item.is_type<_type>())            \
    {                                          \
        _vector.push_back(item);               \
    }

Dx12ResourceGroup::Dx12ResourceGroup(ResourceGroupBuilder builder) : m_builder(std::move(builder))
{
    std::vector<ResourceGroupItem> texture_srvs;
    std::vector<ResourceGroupItem> texture_uavs;

    std::vector<ResourceGroupItem> constant_buffer_views;

    std::vector<ResourceGroupItem> buffer_srvs;
    std::vector<ResourceGroupItem> buffer_uavs;

    std::vector<ResourceGroupItem> sampler_states;

    for (const ResourceGroupItem& item : m_builder.get_resources())
    {
        if (false)
        {
        }
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureSrvT, texture_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureUavT, texture_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::ConstantBufferT, constant_buffer_views)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::BufferSrvT, buffer_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::BufferUavT, buffer_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::SamplerT, sampler_states)
        else
        {
            MIZU_UNREACHABLE("ResourceGroupItemT not implemented");
        }
    }

    // Order of these for loops matter, currently order of resources must be:
    // SRV -> UAV -> CBV -> Sampler
    // See Dx12GraphicsPipeline/Dx12ComputePipeline...::create_root_signature and keep order the same.

    for (const ResourceGroupItem& info : texture_srvs)
    {
        const Dx12ShaderResourceView& native_view =
            static_cast<const Dx12ShaderResourceView&>(*info.as_type<ResourceGroupItem::TextureSrvT>().value);

        m_src_range_cpu_handles.push_back(native_view.handle());
        m_src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : buffer_srvs)
    {
        const Dx12ShaderResourceView& native_view =
            static_cast<const Dx12ShaderResourceView&>(*info.as_type<ResourceGroupItem::BufferSrvT>().value);

        m_src_range_cpu_handles.push_back(native_view.handle());
        m_src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : texture_uavs)
    {
        const Dx12UnorderedAccessView& native_view =
            static_cast<const Dx12UnorderedAccessView&>(*info.as_type<ResourceGroupItem::TextureUavT>().value);

        m_src_range_cpu_handles.push_back(native_view.handle());
        m_src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : buffer_uavs)
    {
        const Dx12UnorderedAccessView& native_view =
            static_cast<const Dx12UnorderedAccessView&>(*info.as_type<ResourceGroupItem::BufferUavT>().value);

        m_src_range_cpu_handles.push_back(native_view.handle());
        m_src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : constant_buffer_views)
    {
        const Dx12ConstantBufferView& native_view =
            static_cast<const Dx12ConstantBufferView&>(*info.as_type<ResourceGroupItem::ConstantBufferT>().value);

        m_src_range_cpu_handles.push_back(native_view.handle());
        m_src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : sampler_states)
    {
        const Dx12SamplerState& native_sampler =
            static_cast<const Dx12SamplerState&>(*info.as_type<ResourceGroupItem::SamplerT>().value);

        m_src_sampler_range_cpu_handles.push_back(native_sampler.handle());
        m_src_sampler_range_num_descriptors.push_back(1);
    }
}

size_t Dx12ResourceGroup::get_hash() const
{
    return m_builder.get_hash();
}

void Dx12ResourceGroup::bind_descriptor_table(
    ID3D12GraphicsCommandList7* command,
    Dx12DescriptorHeapGpuCircularBuffer& cbv_srv_uav_heap,
    Dx12DescriptorHeapGpuCircularBuffer& sampler_heap,
    PipelineType pipeline_type) const
{
    const uint32_t cbv_srv_uav_offset =
        cbv_srv_uav_heap.allocate(static_cast<uint32_t>(m_src_range_cpu_handles.size()));

    const D3D12_CPU_DESCRIPTOR_HANDLE cbv_srv_uav_handle = cbv_srv_uav_heap.get_cpu_handle(cbv_srv_uav_offset);
    const D3D12_GPU_DESCRIPTOR_HANDLE cbv_srv_uav_gpu_handle = cbv_srv_uav_heap.get_gpu_handle(cbv_srv_uav_offset);

    const D3D12_CPU_DESCRIPTOR_HANDLE dest_range_cpu_handles[] = {cbv_srv_uav_handle};
    const uint32_t dest_range_sizes[] = {static_cast<uint32_t>(m_src_range_cpu_handles.size())};

    Dx12Context.device->handle()->CopyDescriptors(
        1,
        dest_range_cpu_handles,
        dest_range_sizes,
        static_cast<uint32_t>(m_src_range_cpu_handles.size()),
        m_src_range_cpu_handles.data(),
        m_src_range_num_descriptors.data(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    switch (pipeline_type)
    {
    case PipelineType::Graphics:
        command->SetGraphicsRootDescriptorTable(0, cbv_srv_uav_gpu_handle);
        break;
    case PipelineType::Compute:
        command->SetComputeRootDescriptorTable(0, cbv_srv_uav_gpu_handle);
        break;
    case PipelineType::RayTracing:
        MIZU_UNREACHABLE("Not implemented");
        break;
    }

    if (!m_src_sampler_range_cpu_handles.empty())
    {
        const uint32_t sampler_offset =
            sampler_heap.allocate(static_cast<uint32_t>(m_src_sampler_range_cpu_handles.size()));

        const D3D12_CPU_DESCRIPTOR_HANDLE sampler_handle = sampler_heap.get_cpu_handle(sampler_offset);
        const D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu_handle = sampler_heap.get_gpu_handle(sampler_offset);

        const D3D12_CPU_DESCRIPTOR_HANDLE dest_sampler_range_cpu_handles[] = {sampler_handle};
        const uint32_t dest_sampler_range_sizes[] = {static_cast<uint32_t>(m_src_sampler_range_cpu_handles.size())};

        Dx12Context.device->handle()->CopyDescriptors(
            1,
            dest_sampler_range_cpu_handles,
            dest_sampler_range_sizes,
            static_cast<uint32_t>(m_src_sampler_range_cpu_handles.size()),
            m_src_sampler_range_cpu_handles.data(),
            m_src_sampler_range_num_descriptors.data(),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

        switch (pipeline_type)
        {
        case PipelineType::Graphics:
            command->SetGraphicsRootDescriptorTable(1, sampler_gpu_handle);
            break;
        case PipelineType::Compute:
            command->SetComputeRootDescriptorTable(1, sampler_gpu_handle);
            break;
        case PipelineType::RayTracing:
            MIZU_UNREACHABLE("Not implemented");
            break;
        }
    }
}

} // namespace Mizu::Dx12