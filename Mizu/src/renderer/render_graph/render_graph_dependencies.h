#pragma once

#include <unordered_set>
#include <vector>

#include "renderer/render_graph/render_graph_types.h"

namespace Mizu {

class RenderGraphDependencies {
  public:
    RenderGraphDependencies() = default;

    void add_rg_texture2D(RGTextureRef value);
    bool contains_rg_texture2D(RGTextureRef value) const;

  private:
    std::unordered_set<RGTextureRef> m_rg_texture2D_dependencies;
};

} // namespace Mizu
