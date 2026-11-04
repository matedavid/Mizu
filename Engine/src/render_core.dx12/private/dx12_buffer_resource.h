#pragma once

#include "render_core/rhi/buffer_resource.h"

#include "dx12_core.h"
#include "dx12_gpu_heap.h"

namespace Mizu::Dx12
{

class Dx12BufferResource : public BufferResource
{
  public:
    Dx12BufferResource(BufferDescription desc);
    Dx12BufferResource(const BufferDescription& desc, const MemoryPlacement& placement);
    Dx12BufferResource(const SparseBufferDescription& desc);
    ~Dx12BufferResource();

    using BufferResource::set_data;
    uint8_t* get_mapped_memory() const override;

    MemoryRequirements get_memory_requirements() const override;

    const BufferDescription& get_description() const override { return m_description; }
    bool is_sparse() const override { return m_sparse; }

    void get_copyable_footprints(
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* footprints,
        uint32_t* num_rows,
        uint64_t* row_size_in_bytes,
        uint64_t* total_size) const;

    ID3D12Resource* handle() const { return m_resource; }
    D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address() const { return m_resource->GetGPUVirtualAddress(); }

  private:
    ID3D12Resource* m_resource = nullptr;
    bool m_sparse = false;
    uint8_t* m_mapped_memory = nullptr;

    MemoryRequirements m_memory_requirements{};

    BufferDescription m_description{};

    void init_memory_requirements();
};

D3D12_RESOURCE_DESC1 get_dx12_resource_desc(const BufferDescription& desc);
MemoryRequirements get_dx12_buffer_memory_requirements(const BufferDescription& desc);

} // namespace Mizu::Dx12