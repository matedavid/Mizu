#include "render/scene/scene_renderer.h"

#include "render/render_graph/render_graph_blackboard.h"
#include "render/render_graph/render_graph_builder.h"
#include "render/scene/scene_renderer_extensions.h"

namespace Mizu
{

SceneRenderer::SceneRenderer() {}

bool SceneRenderer::init(const RenderModuleSystems& systems)
{
    m_frame_allocator = systems.frame_allocator;

    return true;
}

void SceneRenderer::build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard)
{
    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::FrameBegin, builder, blackboard);

    // ...

    SceneRendererExtensions::execute_extensions(SceneRendererExtensionPoint::FrameEnd, builder, blackboard);
}

} // namespace Mizu