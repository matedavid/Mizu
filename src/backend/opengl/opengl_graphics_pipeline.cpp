#include "opengl_graphics_pipeline.h"

#include "backend/opengl/opengl_shader.h"

namespace Mizu::OpenGL {

OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const Description& desc) : m_description(desc) {
    m_shader = std::dynamic_pointer_cast<OpenGLShader>(m_description.shader);
}

void OpenGLGraphicsPipeline::bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    // TODO:
}

bool OpenGLGraphicsPipeline::bake() {
    // TODO:
}

} // namespace Mizu::OpenGL