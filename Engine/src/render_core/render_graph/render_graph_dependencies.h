#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "render_core/render_graph/render_graph_types.h"

namespace Mizu
{

class RenderGraphDependencies
{
  public:
    RenderGraphDependencies() = default;

    void add(std::string name, RGImageViewRef value);
    [[nodiscard]] bool contains(RGImageViewRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependency_name(RGImageViewRef value) const;

    void add(std::string name, RGBufferRef value);
    [[nodiscard]] bool contains(RGBufferRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependency_name(RGBufferRef value) const;

    [[nodiscard]] std::vector<RGImageViewRef> get_image_view_dependencies() const;
    [[nodiscard]] std::vector<RGBufferRef> get_buffer_dependencies() const;

  private:
    template <typename T>
    struct Dependency
    {
        T value;
        std::string name;
    };

    std::unordered_map<RGImageViewRef, Dependency<RGImageViewRef>> m_image_view_dependencies;
    std::unordered_map<RGBufferRef, Dependency<RGBufferRef>> m_uniform_buffer_dependencies;
};

} // namespace Mizu
