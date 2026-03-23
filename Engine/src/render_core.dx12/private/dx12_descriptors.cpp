#include "dx12_descriptors.h"

namespace Mizu::Dx12
{

//
// Dx12DescriptorHeapCircularBuffer
//

Dx12DescriptorHeapCircularBuffer::Dx12DescriptorHeapCircularBuffer(
    uint32_t num_descriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE type,
    D3D12_DESCRIPTOR_HEAP_FLAGS flags)
    : m_descriptor_heap(num_descriptors, type, flags)
    , m_num_descriptors(num_descriptors)
    , m_head(0)
    , m_type(type)
{
    m_allocation_map = std::vector<bool>(num_descriptors);
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12DescriptorHeapCircularBuffer::allocate()
{
#if MIZU_DEBUG
    uint32_t starting_head = m_head;
    while (m_allocation_map[m_head] && m_head + 1 != starting_head)
    {
        m_head = (m_head + 1) % m_num_descriptors;
    }

    MIZU_ASSERT(!m_allocation_map[m_head], "Descriptor heap circular buffer is full");
#endif

    m_allocation_map[m_head] = true;

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_descriptor_heap.get_cpu_descriptor_handle(m_head);
    m_head = (m_head + 1) % m_num_descriptors;

    return handle;
}

void Dx12DescriptorHeapCircularBuffer::free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    const SIZE_T offset = handle.ptr - m_descriptor_heap.get_cpu_descriptor_handle().ptr;
    const uint32_t increment = Dx12Context.device->handle()->GetDescriptorHandleIncrementSize(m_type);

    const uint32_t index = static_cast<uint32_t>(offset / increment);
    MIZU_ASSERT(index < m_num_descriptors, "Descriptor handle out of range");

    MIZU_ASSERT(m_allocation_map[index], "Double free detected in DescriptorHeapCircularBuffer");

    m_allocation_map[index] = false;
}

uint32_t Dx12DescriptorHeapCircularBuffer::get_head() const
{
    return m_head;
}

} // namespace Mizu::Dx12