#pragma once

#include <vector>

#include "dx12_core.h"
#include "dx12_descriptors2.h"

namespace Mizu::Dx12
{

class Dx12DescriptorHeapCircularBuffer
{
  public:
    Dx12DescriptorHeapCircularBuffer(
        uint32_t num_descriptors,
        D3D12_DESCRIPTOR_HEAP_TYPE type,
        D3D12_DESCRIPTOR_HEAP_FLAGS flags);

    D3D12_CPU_DESCRIPTOR_HANDLE allocate();
    void free(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    uint32_t get_head() const;

  private:
    Dx12DescriptorHeap2 m_descriptor_heap;
    uint32_t m_num_descriptors;
    uint32_t m_head;
    D3D12_DESCRIPTOR_HEAP_TYPE m_type;

    std::vector<bool> m_allocation_map;
};

} // namespace Mizu::Dx12