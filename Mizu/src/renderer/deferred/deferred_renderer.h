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
class Cubemap;
class Material;
class Mesh;
class RenderCommandBuffer;
class Scene;

struct DeferredRendererConfig
{
    std::shared_ptr<Cubemap> skybox;
};

class DeferredRenderer : public ISceneRenderer
{
  public:
    DeferredRenderer(std::shared_ptr<Scene> scene, DeferredRendererConfig config, uint32_t width, uint32_t height);
    ~DeferredRenderer() override;

    void render(const Camera& camera) override;
    void resize(uint32_t width, uint32_t height) override;
    void change_config(const DeferredRendererConfig& config);

    std::shared_ptr<Texture2D> get_result_texture() const override { return m_result_texture; }
    std::shared_ptr<Semaphore> get_render_semaphore() const override { return m_render_semaphore; }

  private:
    std::shared_ptr<Scene> m_scene;
    DeferredRendererConfig m_config;
    glm::uvec2 m_dimensions;

    std::shared_ptr<RenderGraphDeviceMemoryAllocator> m_rg_allocator;

    std::shared_ptr<UniformBuffer> m_camera_ubo;
    std::shared_ptr<Texture2D> m_result_texture;

    std::vector<PointLight> m_point_lights;

    RenderGraph m_graph;
    std::shared_ptr<RenderCommandBuffer> m_command_buffer;

    std::shared_ptr<Fence> m_fence;
    std::shared_ptr<Semaphore> m_render_semaphore;

    struct RenderableMeshInfo
    {
        std::shared_ptr<Mesh> mesh;
        std::shared_ptr<Material> material;
        glm::mat4 transform;
    };
    std::vector<RenderableMeshInfo> m_renderable_meshes_info;

    void get_renderable_meshes();
    void get_lights();

    void add_depth_prepass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
    void add_skybox_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard) const;
};

} // namespace Mizu
