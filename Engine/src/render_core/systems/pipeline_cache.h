#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/compute_pipeline.h"
#include "render_core/rhi/graphics_pipeline.h"
#include "render_core/rhi/rtx/ray_tracing_pipeline.h"

namespace Mizu
{

class PipelineCache
{
  public:
    PipelineCache() = default;

    std::shared_ptr<GraphicsPipeline> get_pipeline(const GraphicsPipeline::Description& desc);
    std::shared_ptr<ComputePipeline> get_pipeline(const ComputePipeline::Description& desc);
    std::shared_ptr<RayTracingPipeline> get_pipeline(const RayTracingPipeline::Description& desc);

  private:
    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> m_graphics_cache;
    std::unordered_map<size_t, std::shared_ptr<ComputePipeline>> m_compute_cache;
    std::unordered_map<size_t, std::shared_ptr<RayTracingPipeline>> m_ray_tracing_cache;

    size_t hash(const GraphicsPipeline::Description& desc) const;
    size_t hash(const ComputePipeline::Description& desc) const;
    size_t hash(const RayTracingPipeline::Description& desc) const;
};

} // namespace Mizu