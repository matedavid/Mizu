#include "dx12_resource_group.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_resource_view.h"
#include "render_core/rhi/backend/directx12/dx12_sampler_state.h"

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

    std::vector<ResourceGroupItem> structured_buffer_srvs;
    std::vector<ResourceGroupItem> structured_buffer_uavs;

    std::vector<ResourceGroupItem> sampler_states;

    for (const ResourceGroupItem& item : m_builder.get_resources())
    {
        if (false)
        {
        }
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureSrvT, texture_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::TextureUavT, texture_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::ConstantBufferT, constant_buffer_views)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::StructuredBufferSrvT, structured_buffer_srvs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::StructuredBufferUavT, structured_buffer_uavs)
        BUILDER_ITEM_TYPE_CASE(ResourceGroupItem::SamplerT, sampler_states)
        else
        {
            MIZU_UNREACHABLE("ResourceGroupItemT not implemented");
        }
    }

    // clang-format off
    const uint64_t num_descriptors =   texture_srvs.size()
                                     + texture_uavs.size()
                                     + constant_buffer_views.size()
                                     + structured_buffer_srvs.size()
                                     + structured_buffer_uavs.size();
    // clang-format on

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = static_cast<uint32_t>(num_descriptors);
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));

    if (!sampler_states.empty())
    {
        D3D12_DESCRIPTOR_HEAP_DESC sampler_heap_desc{};
        sampler_heap_desc.NumDescriptors = static_cast<uint32_t>(sampler_states.size());
        sampler_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        sampler_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        sampler_heap_desc.NodeMask = 0;

        DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(
            &sampler_heap_desc, IID_PPV_ARGS(&m_sampler_descriptor_heap)));
    }

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_range_cpu_handles;
    std::vector<uint32_t> src_range_num_descriptors;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_sampler_range_cpu_handles;
    std::vector<uint32_t> src_sampler_range_num_descriptors;

    for (const ResourceGroupItem& info : texture_srvs)
    {
        const Dx12ShaderResourceView& native_view =
            static_cast<const Dx12ShaderResourceView&>(*info.as_type<ResourceGroupItem::TextureSrvT>().value);

        src_range_cpu_handles.push_back(native_view.handle());
        src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : texture_uavs)
    {
        const Dx12UnorderedAccessView& native_view =
            static_cast<const Dx12UnorderedAccessView&>(*info.as_type<ResourceGroupItem::TextureUavT>().value);

        src_range_cpu_handles.push_back(native_view.handle());
        src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : constant_buffer_views)
    {
        const Dx12ConstantBufferView& native_view =
            static_cast<const Dx12ConstantBufferView&>(*info.as_type<ResourceGroupItem::ConstantBufferT>().value);

        src_range_cpu_handles.push_back(native_view.handle());
        src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : structured_buffer_srvs)
    {
        const Dx12ShaderResourceView& native_view =
            static_cast<const Dx12ShaderResourceView&>(*info.as_type<ResourceGroupItem::StructuredBufferSrvT>().value);

        src_range_cpu_handles.push_back(native_view.handle());
        src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : structured_buffer_uavs)
    {
        const Dx12UnorderedAccessView& native_view =
            static_cast<const Dx12UnorderedAccessView&>(*info.as_type<ResourceGroupItem::StructuredBufferUavT>().value);

        src_range_cpu_handles.push_back(native_view.handle());
        src_range_num_descriptors.push_back(1);
    }

    for (const ResourceGroupItem& info : sampler_states)
    {
        const Dx12SamplerState& native_sampler =
            static_cast<const Dx12SamplerState&>(*info.as_type<ResourceGroupItem::SamplerT>().value);

        src_sampler_range_cpu_handles.push_back(native_sampler.handle());
        src_sampler_range_num_descriptors.push_back(1);
    }

    const D3D12_CPU_DESCRIPTOR_HANDLE dest_range_cpu_handles[] = {
        m_descriptor_heap->GetCPUDescriptorHandleForHeapStart()};
    const uint32_t dest_range_sizes[] = {static_cast<uint32_t>(num_descriptors)};

    Dx12Context.device->handle()->CopyDescriptors(
        1,
        dest_range_cpu_handles,
        dest_range_sizes,
        static_cast<uint32_t>(src_range_cpu_handles.size()),
        src_range_cpu_handles.data(),
        src_range_num_descriptors.data(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (!sampler_states.empty())
    {
        const D3D12_CPU_DESCRIPTOR_HANDLE dest_sampler_range_cpu_handles[] = {
            m_sampler_descriptor_heap->GetCPUDescriptorHandleForHeapStart()};
        const uint32_t dest_sampler_range_sizes[] = {static_cast<uint32_t>(sampler_states.size())};

        Dx12Context.device->handle()->CopyDescriptors(
            1,
            dest_sampler_range_cpu_handles,
            dest_sampler_range_sizes,
            static_cast<uint32_t>(src_sampler_range_cpu_handles.size()),
            src_sampler_range_cpu_handles.data(),
            src_sampler_range_num_descriptors.data(),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
}

Dx12ResourceGroup::~Dx12ResourceGroup()
{
    m_descriptor_heap->Release();

    if (m_sampler_descriptor_heap != nullptr)
    {
        m_sampler_descriptor_heap->Release();
    }
}

size_t Dx12ResourceGroup::get_hash() const
{
    return m_builder.get_hash();
}

void Dx12ResourceGroup::bind_descriptor_table(ID3D12GraphicsCommandList4* command, uint32_t set) const
{
    // TODO: The set parameter is not correct. The value in SetGraphicsRootDescriptorTable has to correspond to the
    // index in the root signature of the Pipeline, not the set (described by space0). Final implementation will most
    // likely be related (space0 is the first root parameter), but becase samplers have to bee a different descriptor
    // table, will probably not map 1:1.

    if (m_sampler_descriptor_heap != nullptr)
    {
        ID3D12DescriptorHeap* heaps[] = {m_descriptor_heap, m_sampler_descriptor_heap};
        command->SetDescriptorHeaps(2, heaps);
    }
    else
    {
        ID3D12DescriptorHeap* heaps[] = {m_descriptor_heap};
        command->SetDescriptorHeaps(1, heaps);
    }

    const D3D12_GPU_DESCRIPTOR_HANDLE descriptor_table_gpu_handle =
        m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    command->SetGraphicsRootDescriptorTable(set, descriptor_table_gpu_handle);

    if (m_sampler_descriptor_heap != nullptr)
    {
        const D3D12_GPU_DESCRIPTOR_HANDLE sampler_descriptor_table_gpu_handle =
            m_sampler_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
        command->SetGraphicsRootDescriptorTable(set + 1, sampler_descriptor_table_gpu_handle);
    }
}

} // namespace Mizu::Dx12