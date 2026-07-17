#include "render/scene/scene_renderer_extensions.h"

namespace Mizu
{

static SceneRendererExtensions s_scene_renderer_extensions{};

void SceneRendererExtensions::register_extension(
    SceneRendererExtensionPoint point,
    SceneRendererExtensionPointFunc&& func)
{
    s_scene_renderer_extensions.register_extension_internal(point, std::move(func));
}

void SceneRendererExtensions::execute_extensions(
    SceneRendererExtensionPoint point,
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard)
{
    s_scene_renderer_extensions.execute_extensions_internal(point, builder, blackboard);
}

void SceneRendererExtensions::register_extension_internal(
    SceneRendererExtensionPoint point,
    SceneRendererExtensionPointFunc&& func)
{
    MIZU_ASSERT(point != SceneRendererExtensionPoint::Count, "Can't register SceneRendererExtensionPoint::Count");

    const size_t point_idx = static_cast<size_t>(point);
    m_extensions[point_idx].push_back(std::move(func));
}

void SceneRendererExtensions::execute_extensions_internal(
    SceneRendererExtensionPoint point,
    RenderGraphBuilder& builder,
    RenderGraphBlackboard& blackboard)
{
    const size_t point_idx = static_cast<size_t>(point);
    const SceneRendererPointExtensions& extensions = m_extensions[point_idx];

    for (const SceneRendererExtensionPointFunc& func : extensions)
    {
        func(builder, blackboard);
    }
}

SceneRendererModuleContainer::~SceneRendererModuleContainer()
{
    for (auto& [_, mod] : m_modules_map)
    {
        mod->shutdown();
        delete mod;
    }
}

} // namespace Mizu
