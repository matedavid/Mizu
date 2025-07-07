#include "render_graph_renderer.h"

#include "renderer/camera.h"

#include "render_core/render_graph/render_graph_builder.h"
#include "render_core/resources/texture.h"

#include "state_manager/camera_state_manager.h"
#include "state_manager/static_mesh_state_manager.h"

namespace Mizu
{

RenderGraphRenderer::RenderGraphRenderer() {}

void RenderGraphRenderer::build(RenderGraphBuilder& builder, const CameraDynamicState& camera, const Texture2D& output)
{
    (void)builder;
    (void)camera;
    (void)output;

    // TODO: Implement
}

} // namespace Mizu
