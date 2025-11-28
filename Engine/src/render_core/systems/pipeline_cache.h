#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/pipeline.h"

namespace Mizu
{

class PipelineCache
{
  public:
    PipelineCache() = default;

    std::shared_ptr<Pipeline> get_pipeline(const GraphicsPipelineDescription& desc);
    std::shared_ptr<Pipeline> get_pipeline(const ComputePipelineDescription& desc);
    std::shared_ptr<Pipeline> get_pipeline(const RayTracingPipelineDescription& desc);

  private:
    std::unordered_map<size_t, std::shared_ptr<Pipeline>> m_pipeline_cache;

    size_t hash(const GraphicsPipelineDescription& desc) const;
    size_t hash(const ComputePipelineDescription& desc) const;
    size_t hash(const RayTracingPipelineDescription& desc) const;
};

} // namespace Mizu