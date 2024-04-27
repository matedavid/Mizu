#include "opengl_graphics_pipeline.h"

#include <algorithm>

#include "utility/logging.h"

#include "backend/opengl/opengl_buffers.h"
#include "backend/opengl/opengl_shader.h"
#include "backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

OpenGLGraphicsPipeline::OpenGLGraphicsPipeline(const Description& desc) : m_description(desc) {
    m_shader = std::dynamic_pointer_cast<OpenGLShader>(m_description.shader);

    // Only getting properties because in OpenGL constants == properties
    for (const auto& property : m_shader->get_properties()) {
        std::string name;
        if (std::holds_alternative<ShaderTextureProperty>(property)) {
            name = std::get<ShaderTextureProperty>(property).name;
        } else if (std::holds_alternative<ShaderUniformBufferProperty>(property)) {
            name = std::get<ShaderUniformBufferProperty>(property).name;
        } else if (std::holds_alternative<ShaderValueProperty>(property)) {
            name = std::get<ShaderValueProperty>(property).name;
        }

        m_uniform_info.insert({name, std::nullopt});
    }
}

void OpenGLGraphicsPipeline::bind([[maybe_unused]] const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    // Set pipeline state
    // TODO:

    // Bind uniforms
    for (const auto& [_, info] : m_uniform_info) {
        if (std::holds_alternative<TextureUniformInfo>(*info)) {
            const auto val = std::get<TextureUniformInfo>(*info);

            glActiveTexture(GL_TEXTURE0 + val.binding);
            glBindTexture(GL_TEXTURE_2D, val.texture_handle);
        } else if (std::holds_alternative<UniformBufferUniformInfo>(*info)) {
            const auto val = std::get<UniformBufferUniformInfo>(*info);
            glBindBufferRange(GL_UNIFORM_BUFFER, val.binding, val.ubo_handle, 0, val.size);
        }
    }
}

bool OpenGLGraphicsPipeline::bake() {
    bool baked = true;
    for (const auto& [name, info] : m_uniform_info) {
        if (!info.has_value()) {
            MIZU_LOG_ERROR("Input '{}' in GraphicsPipeline does not have a value", name);
            baked = false;
        }
    }

    return baked;
}

void OpenGLGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<Texture2D>& texture) {
    const auto info = get_uniform_info(name, OpenGLUniformType::Texture, "Texture");
    if (!info.has_value())
        return;

    const auto& native_texture = std::dynamic_pointer_cast<OpenGLTexture2D>(texture);

    TextureUniformInfo texture_info{};
    texture_info.texture_handle = native_texture->handle();
    texture_info.binding = info->binding;

    m_uniform_info[std::string{name}] = texture_info;
}

void OpenGLGraphicsPipeline::add_input(std::string_view name, const std::shared_ptr<UniformBuffer>& ub) {
    const auto info = get_uniform_info(name, OpenGLUniformType::UniformBuffer, "UniformBuffer");
    if (!info.has_value())
        return;

    const auto native_ub = std::dynamic_pointer_cast<OpenGLUniformBuffer>(ub);

    UniformBufferUniformInfo ub_info{};
    ub_info.ubo_handle = native_ub->handle();
    ub_info.binding = info->binding;
    ub_info.size = native_ub->size();

    m_uniform_info[std::string{name}] = ub_info;
}

bool OpenGLGraphicsPipeline::push_constant([[maybe_unused]] const std::shared_ptr<ICommandBuffer>& command_buffer,
                                           std::string_view name,
                                           uint32_t size,
                                           const void* data) {
    const auto info = get_uniform_info(name, OpenGLUniformType::UniformBuffer, "UniformBuffer");
    if (!info.has_value())
        return false;

    if (info->size != size) {
        MIZU_LOG_ERROR("Size of provided data and size of push constant do not match ({} != {})", info->size, size);
        return false;
    }

    auto constant_it = m_constants.find(std::string{name});
    if (constant_it == m_constants.end()) {
        auto ub = std::make_shared<OpenGLUniformBuffer>(size);
        constant_it = m_constants.insert({std::string{name}, ub}).first;
    }

    constant_it->second->set_data(data);
    glBindBufferRange(GL_UNIFORM_BUFFER, info->binding, constant_it->second->handle(), 0, size);

    return true;
}
std::optional<OpenGLUniformInfo> OpenGLGraphicsPipeline::get_uniform_info(std::string_view name,
                                                                          OpenGLUniformType type,
                                                                          std::string_view type_name) const {
    const auto info = m_shader->get_uniform_info(name);
    if (!info.has_value()) {
        MIZU_LOG_WARNING("Property '{}' not found in GraphicsPipeline", name);
        return std::nullopt;
    }

    if (info->type != type) {
        MIZU_LOG_WARNING("Property '{}' is not of type {}", name, type_name);
        return std::nullopt;
    }

    return info;
}

} // namespace Mizu::OpenGL