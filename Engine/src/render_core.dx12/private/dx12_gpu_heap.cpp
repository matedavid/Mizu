#include "dx12_gpu_heap.h"

#include "base/debug/assert.h"
#include "base/math/math.h"

#include "dx12_context.h"
#include "dx12_debug.h"

namespace Mizu::Dx12
{

Dx12GpuHeap::Dx12GpuHeap(GpuHeapDescription desc) : m_description(std::move(desc))
{
    D3D12_HEAP_TYPE heap_type = D3D12_HEAP_TYPE_DEFAULT;
    switch (m_description.domain)
    {
    case HeapDomain::DeviceLocal:
        heap_type = D3D12_HEAP_TYPE_DEFAULT;
        break;
    case HeapDomain::Upload:
        heap_type = D3D12_HEAP_TYPE_UPLOAD;
        break;
    case HeapDomain::Readback:
        heap_type = D3D12_HEAP_TYPE_READBACK;
        break;
    }

    // https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_heap_flags#remarks
    D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;
    switch (m_description.memory_type)
    {
    case MEMORY_TYPE_BITS_ALL:
        MIZU_ASSERT(
            Dx12Context.device->get_feature_data_options().ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_2,
            "Heaps that allow all resources require resource heap tier 2");
        heap_flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;
        break;
    case MEMORY_TYPE_BITS_BUFFER:
        heap_flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
        break;
    case MEMORY_TYPE_BITS_TEXTURE:
        heap_flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
        break;
    case MEMORY_TYPE_BITS_RT_DS_TEXTURE:
        heap_flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
        break;
    default:
        MIZU_UNREACHABLE("Invalid memory type {} for heap '{}'", m_description.memory_type, m_description.name);
        break;
    }

    D3D12_HEAP_PROPERTIES heap_properties{};
    heap_properties.Type = heap_type;
    heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_properties.CreationNodeMask = 0;
    heap_properties.VisibleNodeMask = 0;

    D3D12_HEAP_DESC heap_desc{};
    heap_desc.SizeInBytes = math::align_up(m_description.size, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
    heap_desc.Properties = heap_properties;
    heap_desc.Alignment = 0; // 0 = default 64 KiB placement alignment
    heap_desc.Flags = heap_flags;

    DX12_CHECK(Dx12Context.device->handle()->CreateHeap(&heap_desc, IID_PPV_ARGS(&m_heap)));

    if (!m_description.name.empty())
    {
        DX12_DEBUG_SET_RESOURCE_NAME(m_heap, m_description.name);
    }
}

Dx12GpuHeap::~Dx12GpuHeap()
{
    m_heap->Release();
}

bool Dx12GpuHeap::is_compatible(const MemoryRequirements& reqs) const
{
    return (reqs.memory_type_bits & m_description.memory_type) != 0;
}

} // namespace Mizu::Dx12
