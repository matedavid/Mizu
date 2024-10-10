#include "opengl_resource_group.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<Texture2D> texture) {
    const auto native_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);
    m_texture_resources.insert({std::string{name}, native_texture});
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<Cubemap> cubemap) {
    // TODO:
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ubo);
    m_ubo_resources.insert({std::string{name}, native_ubo});
}

bool OpenGLResourceGroup::bake(const std::shared_ptr<IShader>& shader, [[maybe_unused]] uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<OpenGLShaderBase>(shader);

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
    }

    m_baked = resources_valid;

    return resources_valid;
}

void OpenGLResourceGroup::bind(const std::shared_ptr<IShader>& shader) const {
    if (!m_baked) {
        MIZU_LOG_ERROR("Could not bind resources because ResourceGroup was not baked");
        return;
    }

    const auto native_shader = std::dynamic_pointer_cast<OpenGLShaderBase>(shader);

    for (const auto& [name, texture] : m_texture_resources) {
        const auto info = native_shader->get_property(name);
        MIZU_ASSERT(info.has_value(), "If baked, property should exist");

        // Bind texture
        const auto& texture_info = std::get<ShaderTextureProperty>(info->value);
        if (texture_info.type == ShaderTextureProperty::Type::Storage) {
            const auto& [internal, _, __] = OpenGLTexture2D::get_format(texture->get_format());
            glBindImageTexture(info->binding_info.binding, texture->handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, internal);
        } else if (texture_info.type == ShaderTextureProperty::Type::Sampled) {
            const auto location = native_shader->get_uniform_location(name);
            MIZU_ASSERT(location.has_value(), "Texture uniform location not valid ({})", name);

            glActiveTexture(GL_TEXTURE0 + info->binding_info.binding);
            glBindTexture(GL_TEXTURE_2D, texture->handle());

            glUniform1i(*location, static_cast<GLint>(info->binding_info.binding));
        }
    }

    for (const auto& [name, ubo] : m_ubo_resources) {
        const auto info = native_shader->get_property(name);
        MIZU_ASSERT(info.has_value(), "If baked, property should exist");

        // Bind uniform buffer
        glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle());

        const auto binding_point = native_shader->get_uniform_location(name);
        MIZU_ASSERT(binding_point.has_value(), "Uniform buffer binding point invalid");
        glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(*binding_point), ubo->handle());
    }
}

} // namespace Mizu::OpenGL
