#include "dx12_descriptors2.h"

#include <algorithm>

#include "dx12_context.h"
#include "dx12_resource_view.h"
#include "dx12_sampler_state.h"
#include "dx12_shader.h"

namespace Mizu::Dx12
{

//
// Dx12DescriptorHeap
//

Dx12DescriptorHeap2::Dx12DescriptorHeap2(
    uint32_t num_descriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    : m_num_descriptors(num_descriptors)
    , m_heap_type(type)
    , m_flags(flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = m_num_descriptors;
    heap_desc.Type = m_heap_type;
    heap_desc.Flags = m_flags;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));
}

Dx12DescriptorHeap2::~Dx12DescriptorHeap2()
{
    m_descriptor_heap->Release();
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap2::get_cpu_descriptor_handle(uint32_t offset) const
{
    const uint32_t increment = Dx12Context.device->handle()->GetDescriptorHandleIncrementSize(m_heap_type);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += offset * increment;

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap2::get_gpu_descriptor_handle(uint32_t offset) const
{
    MIZU_ASSERT(
        m_flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        "Can't get gpu descriptor handle for non shader visible heap");

    const uint32_t increment = Dx12Context.device->handle()->GetDescriptorHandleIncrementSize(m_heap_type);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += offset * increment;

    return handle;
}

//
// Dx12DescriptorSet
//

Dx12DescriptorSet::Dx12DescriptorSet(
    Dx12DescriptorAllocation resource_allocation,
    Dx12DescriptorAllocation sampler_allocation,
    Dx12DescriptorManager& manager,
    DescriptorSetAllocationType type)
    : m_resource_allocation(resource_allocation)
    , m_sampler_allocation(sampler_allocation)
    , m_manager(manager)
    , m_type(type)
{
}

Dx12DescriptorSet::~Dx12DescriptorSet()
{
    switch (m_type)
    {
    case DescriptorSetAllocationType::Transient:
        break;
    case DescriptorSetAllocationType::Persistent:
        m_manager.free_persistent(*this);
        break;
    case DescriptorSetAllocationType::Bindless:
        m_manager.free_bindless(*this);
        break;
    }
}

void Dx12DescriptorSet::update(std::span<WriteDescriptor> writes, uint32_t array_offset)
{
    // TODO: Implement correct usage of array_offset
    (void)array_offset;

    std::vector<WriteDescriptor> vec_writes(writes.begin(), writes.end());
    std::sort(vec_writes.begin(), vec_writes.end(), [](const WriteDescriptor& a, const WriteDescriptor& b) {
        // TODO: This logic is duplicated in dx12_root_signature.cpp, should this be unified?
        if (a.binding != b.binding)
        {
            return a.binding < b.binding;
        }

        // Order is: SRV -> UAV -> CBV
        const D3D12_DESCRIPTOR_RANGE_TYPE a_type = Dx12Shader::get_dx12_descriptor_type(a.type);
        const D3D12_DESCRIPTOR_RANGE_TYPE b_type = Dx12Shader::get_dx12_descriptor_type(b.type);

        return a_type < b_type;
    });

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_resource_cpu_handles;
    std::vector<uint32_t> src_resource_num_descriptors;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> src_sampler_cpu_handles;
    std::vector<uint32_t> src_sampler_num_descriptors;

    const auto add_resource_view_handle =
        [&](const WriteDescriptor& write, [[maybe_unused]] ResourceViewType type, bool is_texture) {
            const ResourceView& view = std::get<ResourceView>(write.value);
            MIZU_ASSERT(view.view_type == type, "Invalid resource view type for ResourceViw");

            D3D12_CPU_DESCRIPTOR_HANDLE handle;
            if (is_texture)
            {
                const Dx12ImageResourceView* internal_view = get_internal_image_resource_view(view);
                handle = internal_view->handle;
            }
            else
            {
                const Dx12BufferResourceView* internal_view = get_internal_buffer_resource_view(view);
                handle = internal_view->handle;
            }

            src_resource_cpu_handles.push_back(handle);
        };

    for (const WriteDescriptor& w : vec_writes)
    {
        switch (w.type)
        {
        case ShaderResourceType::TextureSrv:
            add_resource_view_handle(w, ResourceViewType::ShaderResourceView, true);
            break;
        case ShaderResourceType::TextureUav:
            add_resource_view_handle(w, ResourceViewType::UnorderedAccessView, true);
            break;
        case ShaderResourceType::StructuredBufferSrv:
        case ShaderResourceType::ByteAddressBufferSrv:
            add_resource_view_handle(w, ResourceViewType::ShaderResourceView, false);
            break;
        case ShaderResourceType::StructuredBufferUav:
        case ShaderResourceType::ByteAddressBufferUav:
            add_resource_view_handle(w, ResourceViewType::UnorderedAccessView, false);
            break;
        case ShaderResourceType::ConstantBuffer:
            add_resource_view_handle(w, ResourceViewType::ConstantBufferView, false);
            break;
        case ShaderResourceType::AccelerationStructure:
            MIZU_UNREACHABLE("Not implemented");
            break;
        case ShaderResourceType::SamplerState: {
            const Dx12SamplerState& native_sampler =
                static_cast<const Dx12SamplerState&>(*std::get<std::shared_ptr<SamplerState>>(w.value));
            src_sampler_cpu_handles.push_back(native_sampler.handle());
            break;
        }
        case ShaderResourceType::PushConstant:
            MIZU_UNREACHABLE("PushConstant is invalid in this context");
            continue;
        }
    }

    src_resource_num_descriptors.resize(src_resource_cpu_handles.size());
    std::fill(src_resource_num_descriptors.begin(), src_resource_num_descriptors.end(), 1);

    D3D12_CPU_DESCRIPTOR_HANDLE dest_resource_cpu_handles[] = {get_resource_cpu_handle()};
    uint32_t dest_resource_num_descriptors[] = {static_cast<uint32_t>(src_resource_num_descriptors.size())};

    Dx12Context.device->handle()->CopyDescriptors(
        1,
        dest_resource_cpu_handles,
        dest_resource_num_descriptors,
        static_cast<uint32_t>(src_resource_cpu_handles.size()),
        src_resource_cpu_handles.data(),
        src_resource_num_descriptors.data(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    if (!src_sampler_cpu_handles.empty())
    {
        src_sampler_num_descriptors.resize(src_sampler_cpu_handles.size());
        std::fill(src_sampler_num_descriptors.begin(), src_sampler_num_descriptors.end(), 1);

        D3D12_CPU_DESCRIPTOR_HANDLE dest_sampler_cpu_handles[] = {get_sampler_cpu_handle()};
        uint32_t dest_sampler_num_descriptors[] = {static_cast<uint32_t>(src_sampler_num_descriptors.size())};

        Dx12Context.device->handle()->CopyDescriptors(
            1,
            dest_sampler_cpu_handles,
            dest_sampler_num_descriptors,
            static_cast<uint32_t>(src_sampler_cpu_handles.size()),
            src_sampler_cpu_handles.data(),
            src_sampler_num_descriptors.data(),
            D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorSet::get_resource_cpu_handle() const
{
    return m_resource_allocation.descriptor_heap->get_cpu_descriptor_handle(m_resource_allocation.offset);
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorSet::get_resource_gpu_handle() const
{
    return m_resource_allocation.descriptor_heap->get_gpu_descriptor_handle(m_resource_allocation.offset);
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorSet::get_sampler_cpu_handle() const
{
    if (m_sampler_allocation.count == 0)
        return D3D12_CPU_DESCRIPTOR_HANDLE{};

    return m_sampler_allocation.descriptor_heap->get_cpu_descriptor_handle(m_sampler_allocation.offset);
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorSet::get_sampler_gpu_handle() const
{
    if (m_sampler_allocation.count == 0)
        return D3D12_GPU_DESCRIPTOR_HANDLE{};

    return m_sampler_allocation.descriptor_heap->get_gpu_descriptor_handle(m_sampler_allocation.offset);
}

//
// Dx12TransientDescriptorManager
//

Dx12TransientDescriptorManager::Dx12TransientDescriptorManager(uint32_t offset, uint32_t count)
    : m_offset(offset)
    , m_count(count)
    , m_current_head(m_offset)
{
}

uint32_t Dx12TransientDescriptorManager::allocate(uint32_t count)
{
    if (count == 0)
        return 0;

    MIZU_ASSERT(
        m_current_head + count < m_count,
        "Can't allocate {} descriptors, head would be at {} when the number of descriptors is {}",
        count,
        m_current_head + count,
        m_count);

    const uint32_t offset = m_current_head;
    m_current_head = (m_current_head + count) % m_count;

    return offset;
}

void Dx12TransientDescriptorManager::reset()
{
    m_current_head = m_offset;
}

//
// Dx12FreeListDescriptorManager
//

Dx12FreeListDescriptorManager::Dx12FreeListDescriptorManager(uint32_t offset, uint32_t count)
    : m_offset(offset)
    , m_count(count)
{
    m_free_ranges.push_back({.offset = m_offset, .count = m_count});
}

uint32_t Dx12FreeListDescriptorManager::allocate(uint32_t count)
{
    for (size_t i = 0; i < m_free_ranges.size(); ++i)
    {
        FreeRange& range = m_free_ranges[i];

        if (range.count >= count)
        {
            uint32_t allocate_offset = range.offset;

            range.offset += count;
            range.count -= count;

            if (range.count == 0)
            {
                m_free_ranges.erase(m_free_ranges.begin() + i);
            }

            return allocate_offset;
        }
    }

    MIZU_ASSERT(false, "Failed to allocate DescriptorSet");
    return 0;
}

void Dx12FreeListDescriptorManager::free(uint32_t offset, uint32_t count)
{
    insert_and_merge(FreeRange{.offset = offset, .count = count});
}

void Dx12FreeListDescriptorManager::insert_and_merge(FreeRange range)
{
    MIZU_ASSERT(range.count > 0, "Can't insert and merge empty range");

    if (m_free_ranges.empty())
    {
        m_free_ranges.push_back(range);
        return;
    }

    const auto lower_it = std::lower_bound(
        m_free_ranges.begin(), m_free_ranges.end(), range.offset, [](const FreeRange& a, uint32_t offset) {
            return a.offset < offset;
        });

    const size_t index = static_cast<size_t>(std::distance(m_free_ranges.begin(), lower_it));

    // Try merge with prev
    if (index > 0)
    {
        FreeRange& prev = m_free_ranges[index - 1];

        if (prev.offset + prev.count == range.offset)
        {
            prev.count += range.count;

            if (index < m_free_ranges.size())
            {
                FreeRange& next = m_free_ranges[index];
                if (prev.offset + prev.count == next.offset)
                {
                    prev.count += next.count;
                    m_free_ranges.erase(m_free_ranges.begin() + index);
                }
            }

            return;
        }
    }

    // Try merge with next
    if (index < m_free_ranges.size())
    {
        FreeRange& next = m_free_ranges[index];

        if (range.offset + range.count == next.offset)
        {
            next.offset = range.offset;
            next.count += range.count;

            return;
        }
    }

    // Insert at index
    m_free_ranges.insert(m_free_ranges.begin() + index, range);
}

//
// Dx12DescriptorManager
//

Dx12DescriptorManager::Dx12DescriptorManager(const Dx12DescriptorManagerDescription& desc)
{
    const uint32_t total_num_resource_descriptors =
        desc.num_transient_descriptors + desc.num_persistent_descriptors + desc.num_bindless_descriptors;

    const uint32_t total_num_sampler_descriptors = 2048;

    m_resource_descriptor_heap = std::make_unique<Dx12DescriptorHeap2>(
        total_num_resource_descriptors,
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);
    m_sampler_descriptor_heap = std::make_unique<Dx12DescriptorHeap2>(
        total_num_sampler_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE);

    uint32_t transient_heap_offset = 0;
    uint32_t transient_heap_count = desc.num_transient_descriptors;

    uint32_t persistent_heap_offset = transient_heap_offset + transient_heap_count;
    uint32_t persistent_heap_count = desc.num_persistent_descriptors;

    uint32_t bindless_heap_offset = persistent_heap_offset + persistent_heap_count;
    uint32_t bindless_heap_count = desc.num_bindless_descriptors;

    m_resource_transient_manager =
        std::make_unique<Dx12TransientDescriptorManager>(transient_heap_offset, transient_heap_count);
    m_resource_persistent_manager =
        std::make_unique<Dx12FreeListDescriptorManager>(persistent_heap_offset, persistent_heap_count);
    m_resource_bindless_manager =
        std::make_unique<Dx12FreeListDescriptorManager>(bindless_heap_offset, bindless_heap_count);

    uint32_t transient_sampler_heap_offset = 0;
    uint32_t transient_sampler_heap_count =
        static_cast<uint32_t>(0.5f * static_cast<float>(total_num_sampler_descriptors));

    uint32_t persistent_sampler_heap_offset = transient_sampler_heap_offset + transient_sampler_heap_count;
    uint32_t persistent_sampler_heap_count =
        static_cast<uint32_t>(0.5f * static_cast<float>(total_num_sampler_descriptors));

    m_sampler_transient_manager =
        std::make_unique<Dx12TransientDescriptorManager>(transient_sampler_heap_offset, transient_sampler_heap_count);
    m_sampler_persistent_manager =
        std::make_unique<Dx12FreeListDescriptorManager>(persistent_sampler_heap_offset, persistent_sampler_heap_count);
}

void Dx12DescriptorManager::set_descriptor_heaps(ID3D12GraphicsCommandList7* command_list) const
{
    std::array heaps = {
        m_resource_descriptor_heap->handle(),
        m_sampler_descriptor_heap->handle(),
    };

    command_list->SetDescriptorHeaps(static_cast<uint32_t>(heaps.size()), heaps.data());
}

std::shared_ptr<Dx12DescriptorSet> Dx12DescriptorManager::allocate_transient(std::span<DescriptorItem> layout)
{
    uint32_t resource_count = 0, sampler_count = 0;
    get_num_descriptors(layout, resource_count, sampler_count);

    const uint32_t resource_offset = m_resource_transient_manager->allocate(resource_count);
    const uint32_t sampler_offset = m_sampler_transient_manager->allocate(sampler_count);

    const Dx12DescriptorAllocation resource_allocation = {
        .offset = resource_offset,
        .count = resource_count,
        .descriptor_heap = m_resource_descriptor_heap.get(),
    };

    const Dx12DescriptorAllocation sampler_allocation = {
        .offset = sampler_offset,
        .count = sampler_count,
        .descriptor_heap = m_sampler_descriptor_heap.get(),
    };

    return std::make_shared<Dx12DescriptorSet>(
        resource_allocation, sampler_allocation, *this, DescriptorSetAllocationType::Transient);
}

void Dx12DescriptorManager::reset_transient()
{
    m_resource_transient_manager->reset();
    m_sampler_transient_manager->reset();
}

std::shared_ptr<Dx12DescriptorSet> Dx12DescriptorManager::allocate_persistent(std::span<DescriptorItem> layout)
{
    uint32_t resource_count = 0, sampler_count = 0;
    get_num_descriptors(layout, resource_count, sampler_count);

    const uint32_t resource_offset = m_resource_persistent_manager->allocate(resource_count);
    const uint32_t sampler_offset = m_sampler_persistent_manager->allocate(sampler_count);

    const Dx12DescriptorAllocation resource_allocation = {
        .offset = resource_offset,
        .count = resource_count,
        .descriptor_heap = m_resource_descriptor_heap.get(),
    };

    const Dx12DescriptorAllocation sampler_allocation = {
        .offset = sampler_offset,
        .count = sampler_count,
        .descriptor_heap = m_sampler_descriptor_heap.get(),
    };

    return std::make_shared<Dx12DescriptorSet>(
        resource_allocation, sampler_allocation, *this, DescriptorSetAllocationType::Persistent);
}

void Dx12DescriptorManager::free_persistent(const Dx12DescriptorSet& descriptor_set)
{
    const Dx12DescriptorAllocation& resource_allocation = descriptor_set.get_resource_allocation();
    m_resource_persistent_manager->free(resource_allocation.offset, resource_allocation.count);

    const Dx12DescriptorAllocation& sampler_allocation = descriptor_set.get_sampler_allocation();
    if (sampler_allocation.count > 0)
    {
        m_sampler_persistent_manager->free(sampler_allocation.offset, sampler_allocation.count);
    }
}

std::shared_ptr<Dx12DescriptorSet> Dx12DescriptorManager::allocate_bindless(std::span<DescriptorItem> layout)
{
    MIZU_ASSERT(layout.size() == 1, "Currently only supporting bindless descriptor sets with only one binding");

    uint32_t resource_count = 0, sampler_count = 0;
    get_num_descriptors(layout, resource_count, sampler_count);

    MIZU_ASSERT(sampler_count == 0, "Not supporting sampler descriptors with bindless");

    const uint32_t resource_offset = m_resource_bindless_manager->allocate(resource_count);

    // Initialize bindless values with null descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC null_srv_desc{};
    null_srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    null_srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // TODO: Should be generic depending on the layout
    null_srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    null_srv_desc.Texture2D.MipLevels = 1;

    for (uint32_t i = 0; i < resource_count; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_resource_descriptor_heap->get_cpu_descriptor_handle(resource_offset + i);
        Dx12Context.device->handle()->CreateShaderResourceView(nullptr, &null_srv_desc, handle);
    }

    const Dx12DescriptorAllocation resource_allocation = {
        .offset = resource_offset,
        .count = resource_count,
        .descriptor_heap = m_resource_descriptor_heap.get(),
    };

    return std::make_shared<Dx12DescriptorSet>(
        resource_allocation, Dx12DescriptorAllocation{}, *this, DescriptorSetAllocationType::Bindless);
}

void Dx12DescriptorManager::free_bindless(const Dx12DescriptorSet& descriptor_set)
{
    const Dx12DescriptorAllocation& resource_allocation = descriptor_set.get_resource_allocation();
    m_resource_bindless_manager->free(resource_allocation.offset, resource_allocation.count);
}

void Dx12DescriptorManager::get_num_descriptors(
    std::span<DescriptorItem> layout,
    uint32_t& resource_count,
    uint32_t& sampler_count) const
{
    uint32_t num_resources = 0;
    uint32_t num_samplers = 0;

    for (const DescriptorItem& item : layout)
    {
        switch (item.type)
        {
        case ShaderResourceType::TextureSrv:
        case ShaderResourceType::TextureUav:
        case ShaderResourceType::StructuredBufferSrv:
        case ShaderResourceType::StructuredBufferUav:
        case ShaderResourceType::ByteAddressBufferSrv:
        case ShaderResourceType::ByteAddressBufferUav:
        case ShaderResourceType::ConstantBuffer:
            num_resources += item.count;
            break;
        case ShaderResourceType::AccelerationStructure:
            MIZU_UNREACHABLE("Unimplemented");
            continue;
        case ShaderResourceType::SamplerState:
            num_samplers += item.count;
            break;
        case ShaderResourceType::PushConstant:
            MIZU_UNREACHABLE("PushConstant is invalid in this context");
            continue;
        }
    }

    resource_count = num_resources;
    sampler_count = num_samplers;
}

} // namespace Mizu::Dx12