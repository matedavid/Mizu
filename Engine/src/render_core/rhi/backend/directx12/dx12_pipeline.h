#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/pipeline.h"
#include "render_core/shader/shader_group.h"

namespace Mizu::Dx12
{

struct Dx12RootSignatureInfo
{
    uint32_t num_parameters;
    uint32_t root_constant_index;
};

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
    const ShaderGroup& get_shader_group() const { return m_shader_group; }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;
    Dx12RootSignatureInfo m_root_signature_info{};

    ShaderGroup m_shader_group;
    PipelineType m_pipeline_type;
};

} // namespace Mizu::Dx12