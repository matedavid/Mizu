#include "render_graph_dependencies.h"

#include <vector>

namespace Mizu
{

void RenderGraphDependencies::add(std::string name, RGImageViewRef value)
{
    Dependency<RGImageViewRef> dependency{};
    dependency.value = value;
    dependency.name = name;

    m_image_view_dependencies.insert({value, dependency});
}

bool RenderGraphDependencies::contains(RGImageViewRef value) const
{
    return m_image_view_dependencies.contains(value);
}

std::optional<std::string> RenderGraphDependencies::get_dependency_name(RGImageViewRef value) const
{
    const auto& it = m_image_view_dependencies.find(value);
    if (it == m_image_view_dependencies.end())
    {
        return std::nullopt;
    }

    return it->second.name;
}

void RenderGraphDependencies::add(std::string name, RGBufferRef value)
{
    Dependency<RGBufferRef> dependency{};
    dependency.value = value;
    dependency.name = name;

    m_uniform_buffer_dependencies.insert({value, dependency});
}

bool RenderGraphDependencies::contains(RGBufferRef value) const
{
    return m_uniform_buffer_dependencies.contains(value);
}

std::optional<std::string> RenderGraphDependencies::get_dependency_name(RGBufferRef value) const
{
    const auto& it = m_uniform_buffer_dependencies.find(value);
    if (it == m_uniform_buffer_dependencies.end())
    {
        return std::nullopt;
    }

    return it->second.name;
}

std::vector<RGImageViewRef> RenderGraphDependencies::get_image_view_dependencies() const
{
    std::vector<RGImageViewRef> dependencies;
    dependencies.reserve(m_image_view_dependencies.size());

    for (const auto& [_, dep] : m_image_view_dependencies)
    {
        dependencies.push_back(dep.value);
    }

    return dependencies;
}

std::vector<RGBufferRef> RenderGraphDependencies::get_buffer_dependencies() const
{
    std::vector<RGBufferRef> dependencies;
    dependencies.reserve(m_uniform_buffer_dependencies.size());

    for (const auto& [_, dep] : m_uniform_buffer_dependencies)
    {
        dependencies.push_back(dep.value);
    }

    return dependencies;
}

} // namespace Mizu
