#pragma once

#include "render_core/rhi/compute_pipeline.h"

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_pipeline.h"
#include "render_core/shader/shader_group.h"

namespace Mizu::Dx12
{

class Dx12ComputePipeline : public ComputePipeline, public IDx12Pipeline
{
  public:
    Dx12ComputePipeline(const Description& desc);
    ~Dx12ComputePipeline() override;

    ID3D12PipelineState* handle() const override { return m_pipeline_state; }
    ID3D12RootSignature* get_root_signature() const override { return m_root_signature; }
    const Dx12RootSignatureInfo& get_root_signature_info() const override { return m_root_signature_info; }
    const ShaderGroup& get_shader_group() const override { return m_shader_group; }
    Dx12PipelineType get_pipeline_type() const override { return Dx12PipelineType::Compute; }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;

    Dx12RootSignatureInfo m_root_signature_info{};
    ShaderGroup m_shader_group{};
};

} // namespace Mizu::Dx12