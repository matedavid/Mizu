#pragma once

#include "render_core/rhi/pipeline.h"

#include "dx12_core.h"

namespace Mizu::Dx12
{


class Dx12Pipeline : public Pipeline
{
  public:
    Dx12Pipeline(const GraphicsPipelineDescription& desc);
    Dx12Pipeline(const ComputePipelineDescription& desc);
    Dx12Pipeline(const RayTracingPipelineDescription& desc);

    ~Dx12Pipeline() override;

    PipelineType get_pipeline_type() const override { return m_pipeline_type; };

    ID3D12PipelineState* handle() const { return m_pipeline_state; }
    ID3D12RootSignature* get_root_signature() const { return m_root_signature; }
    const Dx12RootSignatureInfo& get_root_signature_info() const { return m_root_signature_info; }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;
    Dx12RootSignatureInfo m_root_signature_info{};

    PipelineType m_pipeline_type;
};

} // namespace Mizu::Dx12