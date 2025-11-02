#pragma once

#include "render_core/rhi/backend/directx12/dx12_core.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/shader/shader_group.h"

namespace Mizu::Dx12
{

class Dx12GraphicsPipeline : public GraphicsPipeline
{
  public:
    Dx12GraphicsPipeline(Description desc);
    ~Dx12GraphicsPipeline() override;

    ID3D12PipelineState* handle() const { return m_pipeline_state; }
    ID3D12RootSignature* get_root_signature() const { return m_root_signature; }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;

    ShaderGroup m_shader_group{};

    void create_root_signature(const ShaderGroup& shader_group);

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