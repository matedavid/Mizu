#include "opengl_resource_group.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_image_resource.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<ImageResource> image_resource) {
    const auto native_resource = std::dynamic_pointer_cast<OpenGLImageResource>(image_resource);
    m_image_resources.insert({std::string(name), native_resource});
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<UniformBuffer> ubo) {
    const auto native_ubo = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ubo);
    m_ubo_resources.insert({std::string(name), native_ubo});
}

bool OpenGLResourceGroup::bake(const std::shared_ptr<IShader>& shader, [[maybe_unused]] uint32_t set) {
    const auto native_shader = std::dynamic_pointer_cast<OpenGLShaderBase>(shader);

    bool resources_valid = true;

    for (const auto& [name, texture] : m_image_resources) {
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

    for (const auto& [name, image] : m_image_resources) {
        const auto info = native_shader->get_property(name);
        MIZU_ASSERT(info.has_value(), "If baked, property should exist");

        const auto& texture_info = std::get<ShaderTextureProperty>(info->value);
        if (texture_info.type == ShaderTextureProperty::Type::Storage) {
            // This path is for storage images (both textures and cubemaps)
            const auto& [internal, _, __] = OpenGLImageResource::get_format_info(image->get_format());
            glBindImageTexture(info->binding_info.binding, image->handle(), 0, GL_FALSE, 0, GL_WRITE_ONLY, internal);
        } else {
            // This path is for texture types and cubemaps
            const auto location = native_shader->get_uniform_location(name);
            MIZU_ASSERT(location.has_value(), "Texture uniform location not valid ({})", name);

            const GLint native_image_type = OpenGLImageResource::get_image_type(image->get_image_type());

            glActiveTexture(GL_TEXTURE0 + info->binding_info.binding);
            glBindTexture(native_image_type, image->handle());

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
