#pragma once

#include <unordered_map>

#include "backend/dx12/dx12_core.h"
#include "render_core/rhi/pipeline_layout.h"

namespace Mizu::Dx12
{

// Forward declarations
struct Dx12RootSignatureInfo;

using PipelineLayoutId = size_t;

class Dx12RootSignatureCache
{
  public:
    Dx12RootSignatureCache() = default;
    ~Dx12RootSignatureCache();

    ID3D12RootSignature* create(PipelineLayoutId id, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& create_info);
    ID3D12RootSignature* get(PipelineLayoutId id) const;
    bool contains(PipelineLayoutId id) const;

  private:
    std::unordered_map<PipelineLayoutId, ID3D12RootSignature*> m_cache;
};

ID3D12RootSignature* create_pipeline_layout(
    std::span<DescriptorBindingInfo> binding_info,
    Dx12RootSignatureInfo& root_signature_info);

} // namespace Mizu::Dx12