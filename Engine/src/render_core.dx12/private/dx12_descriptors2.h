#pragma once

#include <unordered_set>

#include "render_core/rhi/descriptors.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

class Dx12DescriptorHeap2
{
  public:
    Dx12DescriptorHeap2(uint32_t num_descriptors, D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flags);
    ~Dx12DescriptorHeap2();

    D3D12_CPU_DESCRIPTOR_HANDLE get_cpu_descriptor_handle(uint32_t offset = 0) const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_gpu_descriptor_handle(uint32_t offset = 0) const;

    ID3D12DescriptorHeap* handle() const { return m_descriptor_heap; }

  private:
    ID3D12DescriptorHeap* m_descriptor_heap = nullptr;

    uint32_t m_num_descriptors;
    D3D12_DESCRIPTOR_HEAP_TYPE m_heap_type;
    D3D12_DESCRIPTOR_HEAP_FLAGS m_flags;
};

class Dx12DescriptorManager;

struct Dx12DescriptorAllocation
{
    uint32_t offset;
    uint32_t count;
    Dx12DescriptorHeap2* descriptor_heap;
};

class Dx12DescriptorSet : public DescriptorSet
{
  public:
    Dx12DescriptorSet(
        Dx12DescriptorAllocation resource_allocation,
        Dx12DescriptorAllocation sampler_allocation,
        Dx12DescriptorManager& manager,
        DescriptorSetAllocationType type);
    ~Dx12DescriptorSet() override;

    void update(std::span<const WriteDescriptor> writes, uint32_t array_offset = 0) override;

    D3D12_CPU_DESCRIPTOR_HANDLE get_resource_cpu_handle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_resource_gpu_handle() const;

    D3D12_CPU_DESCRIPTOR_HANDLE get_sampler_cpu_handle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_sampler_gpu_handle() const;

    Dx12DescriptorAllocation get_resource_allocation() const { return m_resource_allocation; }
    Dx12DescriptorAllocation get_sampler_allocation() const { return m_sampler_allocation; }

  private:
    Dx12DescriptorAllocation m_resource_allocation;
    Dx12DescriptorAllocation m_sampler_allocation;
    Dx12DescriptorManager& m_manager;
    DescriptorSetAllocationType m_type;
};

class Dx12TransientDescriptorManager
{
  public:
    Dx12TransientDescriptorManager(uint32_t offset, uint32_t count);

    uint32_t allocate(uint32_t count);
    void reset();

  private:
    uint32_t m_offset, m_count;
    uint32_t m_current_head;
};

class Dx12FreeListDescriptorManager
{
  public:
    Dx12FreeListDescriptorManager(uint32_t offset, uint32_t count);

    uint32_t allocate(uint32_t count);
    void free(uint32_t offset, uint32_t count);

  private:
    uint32_t m_offset, m_count;

    struct FreeRange
    {
        uint32_t offset;
        uint32_t count;
    };

    std::vector<FreeRange> m_free_ranges;

    void insert_and_merge(FreeRange range);
};

struct Dx12DescriptorManagerDescription
{
    uint32_t num_transient_descriptors;
    uint32_t num_persistent_descriptors;
    uint32_t num_bindless_descriptors;
};

class Dx12DescriptorManager
{
  public:
    Dx12DescriptorManager(const Dx12DescriptorManagerDescription& desc);
    ~Dx12DescriptorManager() = default;

    void set_descriptor_heaps(ID3D12GraphicsCommandList7* command_list) const;

    std::shared_ptr<Dx12DescriptorSet> allocate_transient(DescriptorSetLayoutHandle layout);
    void reset_transient();

    std::shared_ptr<Dx12DescriptorSet> allocate_persistent(DescriptorSetLayoutHandle layout);
    void free_persistent(const Dx12DescriptorSet& descriptor_set);

    std::shared_ptr<Dx12DescriptorSet> allocate_bindless(DescriptorSetLayoutHandle layout, uint32_t variable_count);
    void free_bindless(const Dx12DescriptorSet& descriptor_set);

  private:
    std::unique_ptr<Dx12DescriptorHeap2> m_resource_descriptor_heap = nullptr;
    std::unique_ptr<Dx12DescriptorHeap2> m_sampler_descriptor_heap = nullptr;

    std::unique_ptr<Dx12TransientDescriptorManager> m_resource_transient_manager = nullptr;
    std::unique_ptr<Dx12FreeListDescriptorManager> m_resource_persistent_manager = nullptr;
    std::unique_ptr<Dx12FreeListDescriptorManager> m_resource_bindless_manager = nullptr;

    std::unique_ptr<Dx12TransientDescriptorManager> m_sampler_transient_manager = nullptr;
    std::unique_ptr<Dx12FreeListDescriptorManager> m_sampler_persistent_manager = nullptr;

    void get_num_descriptors(DescriptorSetLayoutHandle layout, uint32_t& resource_count, uint32_t& sampler_count) const;

#if MIZU_DX12_VALIDATIONS_ENABLED
    std::unordered_set<Dx12DescriptorSet*> m_tracked_transient_resources;

    void transient_descriptor_set_created(Dx12DescriptorSet* descriptor_set);
    void transient_descriptor_set_freed(Dx12DescriptorSet* descriptor_set);
#endif

    friend class Dx12DescriptorSet;
};

} // namespace Mizu::Dx12