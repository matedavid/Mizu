#pragma once

#include <unordered_map>

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/descriptors.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{

struct Dx12DescriptorSetLayoutInfo
{
    static constexpr size_t MAX_RESOURCE_BINDINGS = 10;
    static constexpr size_t MAX_SAMPLER_BINDINGS = 4;

    inplace_vector<DescriptorItem, MAX_RESOURCE_BINDINGS> resource_bindings;
    inplace_vector<DescriptorItem, MAX_SAMPLER_BINDINGS> sampler_bindings;
};

class Dx12DescriptorSetLayoutCache
{
  public:
    DescriptorSetLayoutHandle create(const DescriptorSetLayoutDescription& desc);
    const Dx12DescriptorSetLayoutInfo& get(DescriptorSetLayoutHandle handle) const;
    bool contains(DescriptorSetLayoutHandle handle) const;

  private:
    std::unordered_map<DescriptorSetLayoutHandle, Dx12DescriptorSetLayoutInfo> m_cache;
};

struct Dx12RootSignatureInfo
{
    uint32_t num_parameters;

    uint32_t num_resource_parameters;
    uint32_t num_sampler_parameters;
    uint32_t num_root_constants;

    uint32_t sampler_parameters_offset;
    uint32_t root_constant_offset;
};

class Dx12PipelineLayoutCache
{
  public:
    ~Dx12PipelineLayoutCache();

    PipelineLayoutHandle create(const PipelineLayoutDescription& desc);
    ID3D12RootSignature* get(PipelineLayoutHandle handle) const;
    bool contains(PipelineLayoutHandle handle) const;

    const Dx12RootSignatureInfo& get_root_signature_info(PipelineLayoutHandle handle) const;

  private:
    std::unordered_map<PipelineLayoutHandle, ID3D12RootSignature*> m_cache;
    std::unordered_map<PipelineLayoutHandle, Dx12RootSignatureInfo> m_root_signature_info_cache;
};

} // namespace Mizu::Dx12