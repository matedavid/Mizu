#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"

namespace Mizu
{
class ShaderGroup;
} // namespace Mizu

namespace Mizu::Dx12
{

struct Dx12RootSignatureInfo
{
    uint32_t num_parameters;
    uint32_t root_constant_index;
};

enum class Dx12PipelineType
{
    Graphics,
    Compute,
};

class IDx12Pipeline
{
  public:
    virtual ID3D12PipelineState* handle() const = 0;
    virtual ID3D12RootSignature* get_root_signature() const = 0;
    virtual const Dx12RootSignatureInfo& get_root_signature_info() const = 0;
    virtual const ShaderGroup& get_shader_group() const = 0;
    virtual Dx12PipelineType get_pipeline_type() const = 0;
};

void create_root_signature(
    const ShaderGroup& shader_group,
    Dx12RootSignatureInfo& root_signature_info,
    ID3D12RootSignature*& root_signature);

} // namespace Mizu::Dx12