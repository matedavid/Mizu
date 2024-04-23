#pragma once

#include "graphics_pipeline.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLShader;

class OpenGLGraphicsPipeline : public GraphicsPipeline {
  public:
    explicit OpenGLGraphicsPipeline(const Description& desc);
    ~OpenGLGraphicsPipeline() override = default;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] bool bake() override;

  private:
    std::shared_ptr<OpenGLShader> m_shader;
    Description m_description;
};

} // namespace Mizu::OpenGL
