#include "render_graph_dependencies.h"

namespace Mizu {

void RenderGraphDependencies::add_rg_texture2D(std::string name, RGTextureRef value) {
    m_rg_texture2D_dependencies.insert(value);
    m_rg_texture2D_to_name.insert({value, name});
}

bool RenderGraphDependencies::contains_rg_texture2D(RGTextureRef value) const {
    return m_rg_texture2D_dependencies.contains(value);
}

std::optional<std::string> RenderGraphDependencies::get_dependecy_name(RGTextureRef value) const {
    const auto& it = m_rg_texture2D_to_name.find(value);
    if (it == m_rg_texture2D_to_name.end()) {
        return std::nullopt;
    }

    return it->second;
}

void RenderGraphDependencies::add_rg_uniform_buffer(std::string name, RGUniformBufferRef value) {
    m_rg_uniform_buffer_dependencies.insert(value);
    m_rg_uniform_buffer_to_name.insert({value, name});
}

bool RenderGraphDependencies::contains_rg_uniform_buffer(RGUniformBufferRef value) {
    return m_rg_uniform_buffer_dependencies.contains(value);
}

std::optional<std::string> RenderGraphDependencies::get_dependecy_name(RGUniformBufferRef value) const {
    const auto& it = m_rg_uniform_buffer_to_name.find(value);
    if (it == m_rg_uniform_buffer_to_name.end()) {
        return std::nullopt;
    }

    return it->second;
}

} // namespace Mizu
