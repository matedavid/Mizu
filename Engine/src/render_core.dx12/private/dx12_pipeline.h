#pragma once

#include <memory>

#include "render_core/rhi/pipeline.h"

#include "dx12_buffer_resource.h"
#include "dx12_core.h"
#include "dx12_root_signature.h"

namespace Mizu::Dx12
{

class Dx12Pipeline : public Pipeline
{
  public:
    Dx12Pipeline(const GraphicsPipelineDescription& desc);
    Dx12Pipeline(const ComputePipelineDescription& desc);
    Dx12Pipeline(const RayTracingPipelineDescription& desc);

    ~Dx12Pipeline() override;

    PipelineType get_pipeline_type() const override { return m_pipeline_type; }

    ID3D12PipelineState* handle() const { return m_pipeline_state; }
    ID3D12RootSignature* get_root_signature() const { return m_root_signature; }
    ID3D12CommandSignature* get_draw_indirect_command_signature() const { return m_draw_indirect_command_signature; }
    const Dx12RootSignatureInfo& get_root_signature_info() const { return m_root_signature_info; }

    // RayTracingPipeline Specific
    ID3D12StateObject* get_ray_tracing_state_object() const { return m_ray_tracing_state_object; }
    const D3D12_GPU_VIRTUAL_ADDRESS_RANGE& get_ray_generation_region() const { return m_ray_generation_region; }
    const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& get_miss_region() const { return m_miss_region; }
    const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& get_hit_region() const { return m_hit_region; }
    const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE& get_call_region() const { return m_call_region; }

    D3D12_DISPATCH_RAYS_DESC get_dispatch_rays_desc(uint32_t width, uint32_t height, uint32_t depth = 1) const
    {
        D3D12_DISPATCH_RAYS_DESC desc{};

        desc.RayGenerationShaderRecord = m_ray_generation_region;
        desc.MissShaderTable = m_miss_region;
        desc.HitGroupTable = m_hit_region;
        desc.CallableShaderTable = m_call_region;
        desc.Width = width;
        desc.Height = height;
        desc.Depth = depth;

        return desc;
    }

  private:
    ID3D12PipelineState* m_pipeline_state = nullptr;
    ID3D12RootSignature* m_root_signature = nullptr;
    Dx12RootSignatureInfo m_root_signature_info{};
    ID3D12CommandSignature* m_draw_indirect_command_signature = nullptr;

    PipelineType m_pipeline_type;

    // RayTracingPipeline Specific
    ID3D12StateObject* m_ray_tracing_state_object = nullptr;
    std::unique_ptr<Dx12BufferResource> m_sbt_buffer;
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE m_ray_generation_region{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE m_miss_region{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE m_hit_region{};
    D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE m_call_region{};
};

} // namespace Mizu::Dx12