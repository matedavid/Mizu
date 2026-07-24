#pragma once

#include <span>

#include "render/runtime/game_renderer.h"
#include "render/scene/scene_renderer_extensions.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;
struct RenderGraphResource;

class SceneRenderer : public IRenderModule
{
  public:
    JobHandle create_update_jobs(const RenderModuleUpdateContext& ctx) override;

    void build_render_graph(
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard,
        const RenderModuleFrameData& frame_data) override;

  private:
    SceneRendererModuleContainer m_module_container{};

    void draw_view(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

    void add_views_composition_pass(
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard,
        const RenderModuleFrameData& frame_data,
        std::span<const RenderGraphResource> view_outputs);

    void create_blackboards(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);
    void create_lights_data(RenderGraphBlackboard& blackboard);
};

} // namespace Mizu