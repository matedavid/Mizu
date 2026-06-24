#pragma once

#include <memory>
#include <vector>

#include "render/runtime/game_renderer.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
class Camera;
class ImageResource;
class MaterialResidencySystem;
class RenderGraphBlackboard;
class TextureResidencySystem;

class RenderGraphRenderer : public IRenderModule
{
  public:
    RenderGraphRenderer();

    void set_render_module_systems(const RenderModuleSystems& systems) override;

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) override;

  private:
    // Misc
    std::shared_ptr<BufferResource> m_fullscreen_triangle;

    FrameLinearAllocator* m_frame_allocator = nullptr;
    TextureResidencySystem* m_texture_residency_system = nullptr;
    MaterialResidencySystem* m_material_residency_system = nullptr;

    void render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void get_light_information(RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
