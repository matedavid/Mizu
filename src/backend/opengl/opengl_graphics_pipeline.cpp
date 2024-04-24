#include "opengl_graphics_pipeline.h"

#include "backend/opengl/opengl_shader.h"

namespace Mizu::OpenGL {

OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const Description& desc) : m_description(desc) {
    m_shader = std::dynamic_pointer_cast<OpenGLShader>(m_description.shader);

    // Only getting properties because in OpenGL constants == properties
    for (const auto& property : m_shader->get_properties()) {}
}

void OpenGLGraphicsPipeline::bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    // TODO:
}

bool OpenGLGraphicsPipeline::bake() {
    // TODO:
}

void OpenGLGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) {
    // TODO:
}

void OpenGLGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<UniformBuffer>& ub) {
    // TODO:
}

bool OpenGLGraphicsPipeline::push_constant(const std::shared_ptr<ICommandBuffer>& command_buffer,
                                           std::string_view name,
                                           uint32_t size,
                                           const void* data) {
    // TODO:
    return false;
}

} // namespace Mizu::OpenGL