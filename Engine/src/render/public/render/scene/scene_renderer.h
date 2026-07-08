#pragma once

#include "render/runtime/game_renderer.h"
#include "render/scene/scene_renderer_extensions.h"

namespace Mizu
{

class SceneRenderer : public IRenderModule
{
  public:
    SceneRenderer();

    bool init(const RenderModuleSystems& systems) override;

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) override;

  private:
    FrameLinearAllocator* m_frame_allocator{};

    SceneRendererModuleContainer m_module_container{};
};

} // namespace Mizu