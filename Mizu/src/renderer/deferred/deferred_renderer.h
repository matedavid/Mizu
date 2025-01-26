#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "renderer/scene_renderer.h"

#include "render_core/render_graph/render_graph.h"
#include "render_core/render_graph/render_graph_blackboard.h"
#include "render_core/render_graph/render_graph_builder.h"

namespace Mizu
{

// Forward declarations
class Scene;
class Material;
class Mesh;
class RenderCommandBuffer;

class DeferredRenderer : public ISceneRenderer
{
  public:
    DeferredRenderer(std::shared_ptr<Scene> scene, uint32_t width, uint32_t height);
    ~DeferredRenderer() override;

    void render(const Camera& camera) override;
    void resize(uint32_t width, uint32_t height) override;

    std::shared_ptr<Texture2D> get_result_texture() const override { return m_result_texture; }
    std::shared_ptr<Semaphore> get_render_semaphore() const override { return m_render_semaphore; }

  private:
    std::shared_ptr<Scene> m_scene;
    glm::uvec2 m_dimensions;

    std::shared_ptr<RenderGraphDeviceMemoryAllocator> m_rg_allocator;

    std::shared_ptr<UniformBuffer> m_camera_ubo;
    std::shared_ptr<StorageBuffer> m_point_lights_ssbo;
    std::shared_ptr<Texture2D> m_result_texture;

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
};

} // namespace Mizu