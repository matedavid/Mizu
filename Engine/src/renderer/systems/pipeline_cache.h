#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>

#include "render_core/rhi/pipeline.h"

namespace Mizu
{

// Forward declarations
class ShaderDeclaration;
struct ShaderInstance;

class PipelineCache
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
        const Framebuffer& framebuffer);

    static size_t get_compute_pipeline_hash(size_t compute_hash);

  private:
    std::unordered_map<size_t, std::shared_ptr<Pipeline>> m_cache;
};

std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderDeclaration& vertex,
    const ShaderDeclaration& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    std::shared_ptr<Framebuffer> framebuffer);
std::shared_ptr<Pipeline> get_graphics_pipeline(
    const ShaderInstance& vertex,
    const ShaderInstance& fragment,
    const RasterizationState& raster,
    const DepthStencilState& depth_stencil,
    const ColorBlendState& color_blend,
    std::shared_ptr<Framebuffer> framebuffer);

std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderDeclaration& compute);
std::shared_ptr<Pipeline> get_compute_pipeline(const ShaderInstance& compute);

} // namespace Mizu