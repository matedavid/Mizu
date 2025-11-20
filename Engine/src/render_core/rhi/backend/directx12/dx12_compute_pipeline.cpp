#include "dx12_compute_pipeline.h"

#include "renderer/shader/shader_reflection.h"

#include "render_core/shader/shader_group.h"

#include "render_core/rhi/backend/directx12/dx12_context.h"
#include "render_core/rhi/backend/directx12/dx12_shader.h"

namespace Mizu::Dx12
{

Dx12ComputePipeline::Dx12ComputePipeline(const Description& desc)
{
    // Shader

    MIZU_ASSERT(
        desc.shader != nullptr && desc.shader->get_type() == ShaderType::Compute,
        "No compute shader provided in ComputePipeline");

    const Dx12Shader& native_compute_shader = dynamic_cast<const Dx12Shader&>(*desc.shader);

    m_shader_group = ShaderGroup{};
    m_shader_group.add_shader(native_compute_shader);

    // Root signature
    create_root_signature(m_shader_group, m_root_signature_info, m_root_signature);

    //
    // Create Pipeline
    //
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.CS = native_compute_shader.get_shader_bytecode();
    pso_desc.pRootSignature = m_root_signature;
    pso_desc.NodeMask = 0;

    DX12_CHECK(Dx12Context.device->handle()->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&m_pipeline_state)));
}

Dx12ComputePipeline::~Dx12ComputePipeline()
{
    m_pipeline_state->Release();
    m_root_signature->Release();
}

} // namespace Mizu::Dx12
