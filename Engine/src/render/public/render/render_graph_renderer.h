#pragma once

#include <memory>
#include <vector>

#include "render/draw_block_manager.h"
#include "render/runtime/game_renderer.h"

namespace Mizu
{

// Forward declarations
class BufferResource;
class Camera;
class ImageResource;
class Material;
class Mesh;
class RenderGraphBlackboard;

class RenderGraphRenderer : public IRenderModule
{
  public:
    RenderGraphRenderer();

    void build_render_graph(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) override;

  private:
    // Meshes info
    static constexpr size_t TRANSFORM_INFO_BUFFER_SIZE = 1000;
    std::vector<InstanceTransformInfo> m_transform_info_buffer;

    std::vector<uint64_t> m_main_view_transform_indices_buffer;
    std::vector<uint64_t> m_shadows_view_transform_indices_buffer;

    // Misc
    std::shared_ptr<BufferResource> m_fullscreen_triangle;

    std::unique_ptr<DrawBlockManager> m_draw_manager;

    void render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void get_light_information(RenderGraphBlackboard& blackboard);
    void create_draw_lists(RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
