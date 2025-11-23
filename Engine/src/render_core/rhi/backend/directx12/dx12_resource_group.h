#pragma once

#include "render_core/rhi/resource_group.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
class Dx12DescriptorHeapGpuCircularBuffer;
enum class Dx12PipelineType;

class Dx12ResourceGroup : public ResourceGroup
{
  public:
    Dx12ResourceGroup(ResourceGroupBuilder builder);
    ~Dx12ResourceGroup() override = default;

    size_t get_hash() const override;

    void bind_descriptor_table(
        ID3D12GraphicsCommandList7* command,
        Dx12DescriptorHeapGpuCircularBuffer& cbv_srv_uav_heap,
        Dx12DescriptorHeapGpuCircularBuffer& sampler_heap,
        Dx12PipelineType pipeline_type) const;

  private:
    ResourceGroupBuilder m_builder;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_src_range_cpu_handles;
    std::vector<uint32_t> m_src_range_num_descriptors;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_src_sampler_range_cpu_handles;
    std::vector<uint32_t> m_src_sampler_range_num_descriptors;
};

} // namespace Mizu::Dx12