#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12DescriptorHeap
{
  public:
    Dx12DescriptorHeap(uint32_t num, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    ~Dx12DescriptorHeap();

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle_start() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle_start() const;
    uint32_t get_descriptor_handle_increment_size() const;

    uint32_t get_num_descriptors() const { return m_num_descriptors; }

    ID3D12DescriptorHeap* handle() const { return m_descriptor_heap; }

  private:
    ID3D12DescriptorHeap* m_descriptor_heap = nullptr;

    uint32_t m_num_descriptors;
    D3D12_DESCRIPTOR_HEAP_TYPE m_type;
    D3D12_DESCRIPTOR_HEAP_FLAGS m_flags;
};

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
    Dx12DescriptorHeap m_descriptor_heap;
    uint32_t m_head;

    std::vector<bool> m_allocation_map;
};

class Dx12DescriptorHeapGpuCircularBuffer
{
  public:
    Dx12DescriptorHeapGpuCircularBuffer(uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE type);

    uint32_t allocate(uint32_t num_descriptors);

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle_start() const;
    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_handle(uint32_t offset) const;

    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle_start() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_handle(uint32_t offset) const;

    uint32_t get_head() const;

    ID3D12DescriptorHeap* handle() const { return m_descriptor_heap.handle(); }

  private:
    Dx12DescriptorHeap m_descriptor_heap;
    uint32_t m_head;
};

} // namespace Mizu::Dx12