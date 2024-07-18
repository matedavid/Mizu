#pragma once

#include "renderer/render_graph/render_graph.h"
#include "renderer/system/scene_renderer.h"

namespace Mizu {

// Forward declarations
class Scene;
class RenderCommandBuffer;
class UniformBuffer;

class ForwardRenderer : public ISceneRenderer {
  public:
    explicit ForwardRenderer(std::shared_ptr<Scene> scene, uint32_t width, uint32_t height);

    void render(const Camera& camera) override;
    std::shared_ptr<Texture2D> output_texture() const override { return m_output_texture; }

  private:
    std::shared_ptr<Scene> m_scene;

    RenderGraph m_graph;

    std::shared_ptr<Texture2D> m_output_texture;

    struct CameraInfoUBO {
        glm::mat4 view;
        glm::mat4 projection;
    };
    std::shared_ptr<UniformBuffer> m_camera_info_buffer;

    void init(uint32_t width, uint32_t height);

    void render_models(std::shared_ptr<RenderCommandBuffer> command_buffer) const;
};

} // namespace Mizu
