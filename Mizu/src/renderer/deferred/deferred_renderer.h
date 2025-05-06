#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/lights.h"
#include "renderer/scene_renderer.h"

#include "render_core/render_graph/render_graph.h"
#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"

namespace Mizu
{

// Forward declarations
class Texture2D;
class Environment;
class Material;
class Mesh;
class RenderCommandBuffer;
class Scene;

struct DeferredRendererConfig
{
    // Skybox
    std::shared_ptr<Environment> environment = nullptr;

    // Shadows
    uint32_t num_cascades = 4;
    float cascade_split_lambda = 0.75f;
    float z_scale_factor = 2.0f;

    // SSAO
    bool ssao_enabled = true;
    float ssao_radius = 0.5f;
    bool ssao_blur_enabled = true;
};

class DeferredRenderer : public ISceneRenderer
{
  public:
    DeferredRenderer(std::shared_ptr<Scene> scene, DeferredRendererConfig config);
    ~DeferredRenderer() override;

    void render(const Camera& camera, const Texture2D& output, const CommandBufferSubmitInfo& submit_info) override;
    void change_config(const DeferredRendererConfig& config);

    std::shared_ptr<Semaphore> get_render_semaphore() const override { return m_render_semaphore; }

  private:
    std::shared_ptr<Scene> m_scene;
    DeferredRendererConfig m_config;
    glm::uvec2 m_dimensions{};

    std::shared_ptr<RenderGraphDeviceMemoryAllocator> m_rg_allocator;

    std::shared_ptr<UniformBuffer> m_camera_ubo;

    std::vector<PointLight> m_point_lights;
    std::vector<DirectionalLight> m_directional_lights;

    RenderGraph m_graph;
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    std::shared_ptr<Semaphore> m_render_semaphore;

    struct RenderableMeshInfo
    {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform;
    };
    std::vector<RenderableMeshInfo> m_renderable_meshes_info;

    std::shared_ptr<Texture2D> m_white_texture;

    void get_renderable_meshes();
    void get_lights(const Camera& camera);

    void add_shadowmap_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_ssao_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_skybox_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
};

} // namespace Mizu
