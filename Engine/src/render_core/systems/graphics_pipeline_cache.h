#pragma once

#include <memory>
#include <unordered_map>

#include "render_core/rhi/graphics_pipeline.h"

namespace Mizu
{

class GraphicsPipelineCache
{
  public:
    GraphicsPipelineCache() = default;

    [[nodiscard]] std::shared_ptr<GraphicsPipeline> get_pipeline(const GraphicsPipeline::Description& desc);

  private:
    std::unordered_map<size_t, std::shared_ptr<GraphicsPipeline>> m_cache;

    size_t hash(const GraphicsPipeline::Description& desc) const;
};

} // namespace Mizu