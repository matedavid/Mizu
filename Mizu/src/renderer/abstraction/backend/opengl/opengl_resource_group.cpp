#include "opengl_resource_group.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);
    m_texture_resources.insert({std::string{name}, native_texture});
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ubo);
    m_ubo_resources.insert({std::string{name}, native_ubo});
}

bool OpenGLResourceGroup::bake(const std::shared_ptr<GraphicsShader>& shader, [[maybe_unused]] uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<OpenGLGraphicsShader>(shader);

    bool resources_valid = true;

    for (const auto& [name, texture] : m_texture_resources) {
        const auto info = native_shader->get_property(name);
        if (!info.has_value()) {
            MIZU_LOG_ERROR("Bound resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (!std::holds_alternative<ShaderTextureProperty>(info->value)) {
            MIZU_LOG_ERROR("Bound resource with name {} is not a Texture", name);
            resources_valid = false;
            continue;
        }

        // Bind texture
        const GLint location = glGetUniformLocation(native_shader->handle(), name.data());
        glActiveTexture(GL_TEXTURE0 + info->binding_info.binding);
        glUniform1d(location, info->binding_info.binding);
        glBindTexture(GL_TEXTURE_2D, texture->handle());
    }

    for (const auto& [name, ubo] : m_ubo_resources) {
        const auto info = native_shader->get_property(name);
        if (!info.has_value()) {
            MIZU_LOG_ERROR("Bound resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (!std::holds_alternative<ShaderBufferProperty>(info->value)) {
            MIZU_LOG_ERROR("Bound resource with name {} is not a Uniform Buffer", name);
            resources_valid = false;
            continue;
        }

        const auto buffer_info = std::get<ShaderBufferProperty>(info->value);

        if (buffer_info.total_size != ubo->size()) {
            MIZU_LOG_ERROR("Uniform Buffer with name name {} does not have expected size ({} != {})",
                           name,
                           buffer_info.total_size,
                           ubo->size());
            resources_valid = false;
            continue;
        }

        // Bind uniform buffer
        glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle());

        const GLuint index = glGetUniformBlockIndex(native_shader->handle(), name.data());
        glBindBufferBase(GL_UNIFORM_BUFFER, index, ubo->handle());
    }

    return resources_valid;
}

} // namespace Mizu::OpenGL
