#pragma once

#include "render_core/rhi/buffer_resource.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

class Dx12BufferResource : public BufferResource
{
  public:
    Dx12BufferResource(BufferDescription desc);
    ~Dx12BufferResource();

    MemoryRequirements get_memory_requirements() const override;

    void set_data(const uint8_t* data) const override;

    uint64_t get_size() const override { return m_description.size; }
    BufferUsageBits get_usage() const override { return m_description.usage; }

    const std::string& get_name() const override { return m_description.name; }

    static D3D12_RESOURCE_FLAGS get_dx12_usage(BufferUsageBits usage);
    static D3D12_RESOURCE_STATES get_dx12_buffer_resource_state(BufferResourceState state);

    ID3D12Resource* handle() const { return m_resource; }
    D3D12_GPU_VIRTUAL_ADDRESS get_gpu_address() const { return m_resource->GetGPUVirtualAddress(); }

  private:
    ID3D12Resource* m_resource = nullptr;
    D3D12_RESOURCE_DESC m_buffer_resource_description{};

    BufferDescription m_description{};

    uint8_t* m_mapped_data = nullptr;
};

} // namespace Mizu::Dx12