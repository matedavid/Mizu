#include "render_graph_dependencies.h"

namespace Mizu {

void RenderGraphDependencies::add_rg_texture2D(RGTextureRef value) {
    m_rg_texture2D_dependencies.insert(value);
}

bool RenderGraphDependencies::contains_rg_texture2D(RGTextureRef value) const {
    return m_rg_texture2D_dependencies.contains(value);
}

void RenderGraphDependencies::add_rg_uniform_buffer(RGUniformBufferRef value) {
    m_rg_uniform_buffer_dependencies.insert(value);
}

bool RenderGraphDependencies::contains_rg_uniform_buffer(RGUniformBufferRef value) {
    return m_rg_uniform_buffer_dependencies.contains(value);
}

} // namespace Mizu
