#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/draw_block_manager.h"
#include "renderer/lights.h"
#include "renderer/render_graph_renderer_settings.h"

namespace Mizu
{

// Forward declarations
class Camera;
class Material;
class Mesh;
class RenderGraphBlackboard;
class RenderGraphBuilder;
class ImageResource;
class BufferResource;
struct DirectionalLight;
struct PointLight;

class RenderGraphRenderer
{
  public:
    RenderGraphRenderer();

    void build(RenderGraphBuilder& builder, const Camera& camera, const std::shared_ptr<ImageResource>& output);

  private:
    // Meshes info
    static constexpr size_t TRANSFORM_INFO_BUFFER_SIZE = 1000;
    std::vector<InstanceTransformInfo> m_transform_info_buffer;

    std::vector<uint64_t> m_main_view_transform_indices_buffer;
    std::vector<uint64_t> m_cascaded_shadows_transform_indices_buffer;

    // Lights info
    std::vector<PointLight> m_point_lights;
    std::vector<DirectionalLight> m_directional_lights;

    std::vector<float> m_cascade_splits_factor;
    std::vector<glm::mat4> m_cascade_light_space_matrices;

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

    void get_light_information(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);
    void create_draw_lists(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
