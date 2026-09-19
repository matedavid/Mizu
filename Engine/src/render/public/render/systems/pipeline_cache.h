#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/pipeline.h"
#include "shader/shader_declaration.h"

#include "mizu_render_module.h"

namespace Mizu
{

class MIZU_RENDER_API PipelineCache
{
  public:
    static PipelineCache& get();

    void reset();

    void insert(size_t hash, std::shared_ptr<Pipeline> pipeline);
    std::shared_ptr<Pipeline> get(size_t hash) const;
    bool contains(size_t hash) const;

    static size_t get_graphics_pipeline_hash(
        size_t vertex_hash,
        size_t fragment_hash,
        const RasterizationState& raster,
        const DepthStencilState& depth_stencil,
        const ColorBlendState& color_blend,
        const FramebufferInfo& framebuffer_info);

    static size_t get_compute_pipeline_hash(size_t compute_hash);

    static size_t get_ray_tracing_pipeline_hash(
        size_t raygen_hash,
        size_t miss_hash,
        size_t hit_group_hash,
        uint32_t max_ray_recursion_depth);

  private:
    std::unordered_map<size_t, std::shared_ptr<Pipeline>> m_cache;
};

MIZU_RENDER_API std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderDeclaration& vertex,
    const ShaderDeclaration& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    const FramebufferInfo& framebuffer_info);
MIZU_RENDER_API std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderInstance& vertex,
    const ShaderInstance& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    const FramebufferInfo& framebuffer_info);

MIZU_RENDER_API std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderDeclaration& compute);
MIZU_RENDER_API std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderInstance& compute);

// using RtxShaderDeclarations =
//     inplace_vector<ShaderDeclaration, RayTracingPipelineDescription::MAX_VARIABLE_NUM_SHADERS>;

struct ShaderHitGroupInstance
{
    std::optional<ShaderInstance> closest_hit = std::nullopt;
    std::optional<ShaderInstance> any_hit = std::nullopt;
    std::optional<ShaderInstance> intersection = std::nullopt;
};

using ShaderInstances = inplace_vector<ShaderInstance, RayTracingPipelineDescription::MAX_VARIABLE_NUM_SHADERS>;
using ShaderHitGroupInstances =
    inplace_vector<ShaderHitGroupInstance, RayTracingPipelineDescription::MAX_VARIABLE_NUM_SHADERS>;

// std::shared_ptr<Pipeline> get_ray_tracing_pipeline(
//     const ShaderDeclaration& raygen,
//     const RtxShaderDeclarations& miss,
//     const RtxShaderDeclarations& closest_hit,
//     uint32_t max_ray_recursion_depth);
MIZU_RENDER_API std::shared_ptr<Pipeline> get_ray_tracing_pipeline(
    const ShaderInstance& raygen,
    const ShaderInstances& miss,
    const ShaderHitGroupInstances& hit_groups,
    uint32_t max_ray_recursion_depth);

} // namespace Mizu
