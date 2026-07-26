#pragma once

#include <array>
#include <span>

#include "render/render_settings/render_settings.h"
#include "render/runtime/game_renderer.h"
#include "render/scene/scene_renderer_extensions.h"
#include "render/state_manager/render_view_state_manager.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;
struct RenderGraphResource;

class SceneRenderer : public IRenderModule
{
  public:
    bool init() override;

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
        RenderGraphResource output_texture,
        std::span<const RenderGraphResource> view_outputs);

    void update_view_job(uint32_t view_id);

    void create_view_blackboards(
        RenderGraphBuilder& builder,
        RenderGraphBlackboard& blackboard,
        const RenderViewData& view_data);
    void create_lights_data(RenderGraphBlackboard& blackboard);

    struct RenderViewInfo
    {
        const RenderViewRegistryEntry* entry;
        ResolvedViewRenderSettings render_settings;
    };

    std::array<RenderViewInfo, RenderViewConfig::MaxNumHandles> m_render_views{};
    uint32_t m_num_render_views = 0;
};

} // namespace Mizu