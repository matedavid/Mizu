#include "opengl_resource_group.h"

#include "backend/opengl/opengl_buffers.h"
#include "backend/opengl/opengl_shader.h"
#include "backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);
    m_texture_resources.insert({std::string{name}, native_texture});
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ubo);
    m_ubo_resources.insert({std::string{name}, native_ubo});
}

bool OpenGLResourceGroup::bake(const std::shared_ptr<Shader>& shader, [[maybe_unused]] uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<OpenGLShader>(shader);

    bool resources_valid = true;

    for (const auto& [name, texture] : m_texture_resources) {
        const auto info = native_shader->get_uniform_info(name);
        if (!info.has_value()) {
            MIZU_LOG_ERROR("Bound resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (info->type != OpenGLUniformType::Texture) {
            MIZU_LOG_ERROR("Bound resource with name {} is not a Texture", name);
            resources_valid = false;
            continue;
        }

        // Bind texture
        glActiveTexture(GL_TEXTURE0 + info->binding);
        glUniform1d(info->location, info->binding);
        glBindTexture(GL_TEXTURE_2D, texture->handle());
    }

    for (const auto& [name, ubo] : m_ubo_resources) {
        const auto info = native_shader->get_uniform_info(name);
        if (!info.has_value()) {
            MIZU_LOG_ERROR("Bound resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (info->type != OpenGLUniformType::UniformBuffer) {
            MIZU_LOG_ERROR("Bound resource with name {} is not a Uniform Buffer", name);
            resources_valid = false;
            continue;
        }

        if (info->size != ubo->size()) {
            MIZU_LOG_ERROR("Uniform Buffer with name name {} does not have expected size ({} != {})",
                           name,
                           info->size,
                           ubo->size());
            resources_valid = false;
            continue;
        }

        // Bind uniform buffer
        glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle());
        glBindBufferBase(GL_UNIFORM_BUFFER, info->binding, ubo->handle());
    }

    return resources_valid;
}

} // namespace Mizu::OpenGL
