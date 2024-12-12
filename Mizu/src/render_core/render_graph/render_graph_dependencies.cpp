#include "render_graph_dependencies.h"

#include <vector>

namespace Mizu
{

void RenderGraphDependencies::add(std::string name, RGImageRef value)
{
    Dependency<RGImageRef> dependency{};
    dependency.value = value;
    dependency.name = name;

    m_image_dependencies.insert({value, dependency});
}

bool RenderGraphDependencies::contains(RGImageRef value) const
{
    return m_image_dependencies.contains(value);
}

std::optional<std::string> RenderGraphDependencies::get_dependency_name(RGImageRef value) const
{
    const auto& it = m_image_dependencies.find(value);
    if (it == m_image_dependencies.end())
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

std::vector<RGImageRef> RenderGraphDependencies::get_image_dependencies() const
{
    std::vector<RGImageRef> dependencies;
    dependencies.reserve(m_image_dependencies.size());

    for (const auto& [_, dep] : m_image_dependencies)
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
