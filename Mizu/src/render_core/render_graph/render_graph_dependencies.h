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

    void add(std::string name, RGImageRef value);
    [[nodiscard]] bool contains(RGImageRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependency_name(RGImageRef value) const;

    void add(std::string name, RGBufferRef value);
    [[nodiscard]] bool contains(RGBufferRef value) const;
    [[nodiscard]] std::optional<std::string> get_dependency_name(RGBufferRef value) const;

    [[nodiscard]] std::vector<RGImageRef> get_image_dependencies() const;
    [[nodiscard]] std::vector<RGBufferRef> get_buffer_dependencies() const;

  private:
    template <typename T>
    struct Dependency
    {
        T value;
        std::string name;
    };

    std::unordered_map<RGImageRef, Dependency<RGImageRef>> m_image_dependencies;
    std::unordered_map<RGBufferRef, Dependency<RGBufferRef>> m_uniform_buffer_dependencies;
};

} // namespace Mizu
