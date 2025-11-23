#include "dx12_descriptors.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"

namespace Mizu::Dx12
{

//
// Dx12DescriptorHeap
//

Dx12DescriptorHeap::Dx12DescriptorHeap(uint32_t num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    : m_num_descriptors(num)
    , m_type(type)
    , m_flags(flags)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.NumDescriptors = m_num_descriptors;
    heap_desc.Type = m_type;
    heap_desc.Flags = m_flags;
    heap_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&m_descriptor_heap)));
}

Dx12DescriptorHeap::~Dx12DescriptorHeap()
{
    m_descriptor_heap->Release();
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap::get_cpu_descriptor_handle_start() const
{
    return m_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorHeap::get_gpu_descriptor_handle_start() const
{
    MIZU_ASSERT(
        m_flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        "Can't get GPU handle of non-shader visible descriptor heap");
    return m_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
}

uint32_t Dx12DescriptorHeap::get_descriptor_handle_increment_size() const
{
    return Dx12Context.device->handle()->GetDescriptorHandleIncrementSize(m_type);
}

//
// Dx12DescriptorHeapCircularBuffer
//

Dx12DescriptorHeapCircularBuffer::Dx12DescriptorHeapCircularBuffer(
    uint32_t num_descriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    : m_descriptor_heap(num_descriptors, type, flags)
    , m_head(0)
{
    m_allocation_map = std::vector<bool>(num_descriptors);
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapCircularBuffer::allocate()
{
#if MIZU_DEBUG
    uint32_t starting_head = m_head;
    while (m_allocation_map[m_head])
    {
        m_head = (m_head + 1) % m_descriptor_heap.get_num_descriptors();
    }

    MIZU_ASSERT(m_head == starting_head, "DescriptorHeapCircularBuffer has no available spots");
#endif

    m_allocation_map[m_head] = true;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap.get_cpu_descriptor_handle_start();
    handle.ptr += m_head * m_descriptor_heap.get_descriptor_handle_increment_size();

    m_head = (m_head + 1) % m_descriptor_heap.get_num_descriptors();
    return handle;
}

void Dx12DescriptorHeapCircularBuffer::free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const SIZE_T offset = handle.ptr - m_descriptor_heap.get_cpu_descriptor_handle_start().ptr;

    const uint32_t index = static_cast<uint32_t>(offset / m_descriptor_heap.get_descriptor_handle_increment_size());
    MIZU_ASSERT(index < m_descriptor_heap.get_num_descriptors(), "Descriptor handle out of range");

    MIZU_ASSERT(m_allocation_map[index], "Double free detected in DescriptorHeapCircularBuffer");

    m_allocation_map[index] = false;
}

uint32_t Dx12DescriptorHeapCircularBuffer::get_head() const
{
    return m_head;
}

//
// Dx12DescriptorHeapGpuCircularBuffer
//

Dx12DescriptorHeapGpuCircularBuffer::Dx12DescriptorHeapGpuCircularBuffer(
    uint32_t num_descriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type)
    : m_descriptor_heap(num_descriptors, type, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    , m_head(0)
{
}

uint32_t Dx12DescriptorHeapGpuCircularBuffer::allocate(uint32_t num_descriptors)
{
    if (m_head + num_descriptors > m_descriptor_heap.get_num_descriptors())
    {
        m_head = 0;
    }

    const uint32_t allocation_head = m_head;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap.get_cpu_descriptor_handle_start();
    handle.ptr += allocation_head * m_descriptor_heap.get_descriptor_handle_increment_size();

    m_head += num_descriptors;

    return allocation_head;
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapGpuCircularBuffer::get_cpu_handle_start() const
{
    return m_descriptor_heap.get_cpu_descriptor_handle_start();
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapGpuCircularBuffer::get_cpu_handle(uint32_t offset) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap.get_cpu_descriptor_handle_start();
    handle.ptr += offset * m_descriptor_heap.get_descriptor_handle_increment_size();

    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapGpuCircularBuffer::get_gpu_handle_start() const
{
    return m_descriptor_heap.get_gpu_descriptor_handle_start();
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapGpuCircularBuffer::get_gpu_handle(uint32_t offset) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap.get_gpu_descriptor_handle_start();
    handle.ptr += offset * m_descriptor_heap.get_descriptor_handle_increment_size();

    return handle;
}

} // namespace Mizu::Dx12