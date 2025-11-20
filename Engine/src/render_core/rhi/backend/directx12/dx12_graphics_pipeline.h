#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/backend/directx12/dx12_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/shader/shader_group.h"

namespace Mizu::Dx12
{

class Dx12GraphicsPipeline : public GraphicsPipeline, public IDx12Pipeline
{
  public:
    Dx12GraphicsPipeline(const Description& desc);
    ~Dx12GraphicsPipeline() override;

    ID3D12PipelineState* handle() const override { return m_pipeline_state; }
    ID3D12RootSignature* get_root_signature() const override { return m_root_signature; }
    const Dx12RootSignatureInfo& get_root_signature_info() const override { return m_root_signature_info; }
    const ShaderGroup& get_shader_group() const override { return m_shader_group; }
    Dx12PipelineType get_pipeline_type() const override { return Dx12PipelineType::Graphics; }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;

    Dx12RootSignatureInfo m_root_signature_info{};
    ShaderGroup m_shader_group{};

    // Rasterization helpers
    static D3D12_PRIMITIVE_TOPOLOGY_TYPE get_polygon_mode(RasterizationState::PolygonMode mode);
    static D3D12_CULL_MODE get_cull_mode(RasterizationState::CullMode mode);
    static BOOL get_front_face(RasterizationState::FrontFace mode);

    // Depth Stencil helpers
    static D3D12_COMPARISON_FUNC get_depth_compare_op(DepthStencilState::DepthCompareOp op);

    // Color Blend helpers
    static D3D12_BLEND get_blend_factor(ColorBlendState::BlendFactor factor);
    static D3D12_BLEND_OP get_blend_operation(ColorBlendState::BlendOperation operation);
    static UINT8 get_color_component_flags(ColorBlendState::ColorComponentBits bits);
    static D3D12_LOGIC_OP get_logic_operation(ColorBlendState::LogicOperation operation);
};

} // namespace Mizu::Dx12