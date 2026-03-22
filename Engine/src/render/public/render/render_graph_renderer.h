#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "render/core/lights.h"
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
struct DirectionalLight;
struct PointLight;

class RenderGraphRenderer : public IRenderModule
{
  public:
    RenderGraphRenderer();

    void build_render_graph2(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) override;

  private:
    // Meshes info
    static constexpr size_t TRANSFORM_INFO_BUFFER_SIZE = 1000;
    std::vector<InstanceTransformInfo> m_transform_info_buffer;

    std::vector<uint64_t> m_main_view_transform_indices_buffer;
    std::vector<uint64_t> m_shadows_view_transform_indices_buffer;

    // Lights info
    std::vector<PointLight> m_point_lights;
    std::vector<DirectionalLight> m_directional_lights;

    std::vector<float> m_cascade_splits_factor;
    std::vector<glm::mat4> m_cascade_light_space_matrices;

    // Misc
    std::shared_ptr<BufferResource> m_fullscreen_triangle;

    std::unique_ptr<DrawBlockManager> m_draw_manager;

    void render_scene(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_normals_prepass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;
    void add_light_culling_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;

    void add_light_culling_debug_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_debug_pass(RenderGraphBuilder2& builder, RenderGraphBlackboard& blackboard) const;

    void get_light_information(RenderGraphBlackboard& blackboard);
    void create_draw_lists(RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
