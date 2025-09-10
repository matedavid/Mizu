#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

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
class Texture2D;
class VertexBuffer;
struct DirectionalLight;
struct PointLight;

class RenderGraphRenderer
{
  public:
    RenderGraphRenderer();

    void build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output);

  private:
    // Meshes info
    struct RenderMesh
    {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform;
    };
    std::vector<RenderMesh> m_render_meshes;

    // Lights info
    std::vector<PointLight> m_point_lights;
    std::vector<DirectionalLight> m_directional_lights;

    std::vector<float> m_cascade_splits_factor;
    std::vector<glm::mat4> m_cascade_light_space_matrices;

    // Misc
    std::shared_ptr<VertexBuffer> m_fullscreen_triangle;

    void render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_normals_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_light_culling_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_light_culling_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_cascaded_shadow_mapping_debug_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void get_render_meshes(const Camera& camera);
    void get_light_information(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);
};

} // namespace Mizu
