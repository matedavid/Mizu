#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "renderer/render_graph/render_graph_types.h"

namespace Mizu {

class RenderGraphDependencies {
  public:
    RenderGraphDependencies() = default;

    void add_rg_texture2D(std::string name, RGTextureRef value);
    [[nodiscard]] bool contains_rg_texture2D(RGTextureRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependecy_name(RGTextureRef value) const;

    void add_rg_cubemap(std::string name, RGCubemapRef value);
    [[nodiscard]] bool contains_rg_cubemap(RGCubemapRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependecy_name(RGCubemapRef value) const;

    void add_rg_uniform_buffer(std::string name, RGUniformBufferRef value);
    [[nodiscard]] bool contains_rg_uniform_buffer(RGUniformBufferRef value);
    [[nodiscard]] std::optional<std::string> get_dependecy_name(RGUniformBufferRef value) const;

  private:
    std::unordered_set<RGTextureRef> m_rg_texture2D_dependencies;
    std::unordered_set<RGCubemapRef> m_rg_cubemap_dependencies;
    std::unordered_set<RGUniformBufferRef> m_rg_uniform_buffer_dependencies;

    std::unordered_map<RGTextureRef, std::string> m_rg_texture2D_to_name;
    std::unordered_map<RGCubemapRef, std::string> m_rg_cubemap_to_name;
    std::unordered_map<RGUniformBufferRef, std::string> m_rg_uniform_buffer_to_name;
};

} // namespace Mizu
