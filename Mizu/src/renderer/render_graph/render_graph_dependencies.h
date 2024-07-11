#pragma once

#include <unordered_set>
#include <vector>

#include "renderer/render_graph/render_graph_types.h"

namespace Mizu {

class RenderGraphDependencies {
  public:
    RenderGraphDependencies() = default;

    void add_rg_texture2D(RGTextureRef value);
    [[nodiscard]] bool contains_rg_texture2D(RGTextureRef value) const;

    void add_rg_uniform_buffer(RGUniformBufferRef value);
    [[nodiscard]] bool contains_rg_uniform_buffer(RGUniformBufferRef value);

  private:
    std::unordered_set<RGTextureRef> m_rg_texture2D_dependencies;
    std::unordered_set<RGTextureRef> m_rg_uniform_buffer_dependencies;
};

} // namespace Mizu
