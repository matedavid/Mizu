#pragma once

#include "render_core/rhi/resource_group.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu::Dx12
{

// Forward declarations
enum class Dx12PipelineType;

class Dx12ResourceGroup : public ResourceGroup
{
  public:
    Dx12ResourceGroup(ResourceGroupBuilder builder);
    ~Dx12ResourceGroup() override;

    size_t get_hash() const override;

    void bind_descriptor_table(ID3D12GraphicsCommandList4* command, uint32_t set, Dx12PipelineType pipeline_type) const;

    ID3D12DescriptorHeap* handle() const { return m_descriptor_heap; }
    ID3D12DescriptorHeap* sampler_descriptor_heap() const { return m_sampler_descriptor_heap; }

  private:
    ID3D12DescriptorHeap* m_descriptor_heap = nullptr;
    ID3D12DescriptorHeap* m_sampler_descriptor_heap = nullptr;
    ResourceGroupBuilder m_builder;
};

} // namespace Mizu::Dx12