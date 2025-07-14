#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/lights.h"

namespace Mizu
{

// Forward declarations
class Camera;
class Material;
class Mesh;
class RenderGraphBlackboard;
class RenderGraphBuilder;
class Texture2D;
struct DirectionalLight;
struct PointLight;

class RenderGraphRenderer
{
  public:
    void build(RenderGraphBuilder& builder, const Camera& camera, const Texture2D& output);

  private:
    struct RenderMesh
    {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform;
    };
    std::vector<RenderMesh> m_render_meshes;

    std::vector<PointLight> m_point_lights;
    std::vector<DirectionalLight> m_directional_lights;

    void render_scene(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void add_depth_pre_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_simple_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;

    void get_render_meshes();
    void get_light_information();
};

} // namespace Mizu
