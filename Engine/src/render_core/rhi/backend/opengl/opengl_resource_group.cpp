#include "opengl_resource_group.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/opengl/opengl_image_resource.h"
#include "render_core/rhi/backend/opengl/opengl_shader.h"

#include "utility/assert.h"

namespace Mizu::OpenGL
{

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<ImageResourceView> image_view)
{
    (void)name;
    (void)image_view;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<BufferResource> buffer_resource)
{
    const auto native_buffer_resource = std::dynamic_pointer_cast<OpenGLBufferResource>(buffer_resource);
    m_ubo_resources.insert({std::string(name), native_buffer_resource});
}

void OpenGLResourceGroup::add_resource(std::string_view name, std::shared_ptr<SamplerState> sampler_state)
{
    (void)name;
    (void)sampler_state;

    MIZU_UNREACHABLE("Unimplemented");
}

size_t OpenGLResourceGroup::get_hash() const
{
    std::hash<std::string> string_hasher;
    std::hash<uint32_t> uint32_hasher;
    std::hash<uint64_t> uint64_hasher;

    size_t hash = 0;
    for (const auto& [name, resource] : m_image_resources)
    {
        const size_t dims_hash = uint32_hasher(resource->get_width()) ^ uint32_hasher(resource->get_height())
                                 ^ uint32_hasher(resource->get_depth());
        hash ^= string_hasher(name) ^ uint32_hasher(static_cast<uint32_t>(resource->get_format()))
                ^ uint32_hasher(static_cast<uint32_t>(resource->get_image_type())) ^ dims_hash;
    }

    for (const auto& [name, resource] : m_ubo_resources)
    {
        hash ^= string_hasher(name) ^ uint64_hasher(resource->get_size())
                ^ uint32_hasher(static_cast<uint32_t>(resource->get_type()));
    }

    return hash;
}

bool OpenGLResourceGroup::bake(const IShader& shader, [[maybe_unused]] uint32_t set)
{
    const OpenGLShaderBase& native_shader = dynamic_cast<const OpenGLShaderBase&>(shader);

    bool resources_valid = true;

    for (const auto& [name, texture] : m_image_resources)
    {
        const auto info = native_shader.get_property(name);
        if (!info.has_value())
        {
            MIZU_LOG_ERROR("Resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (!std::holds_alternative<ShaderImageProperty>(info->value))
        {
            MIZU_LOG_ERROR("Resource with name {} is not a Texture", name);
            resources_valid = false;
            continue;
        }
    }

    for (const auto& [name, ubo] : m_ubo_resources)
    {
        const auto info = native_shader.get_property(name);
        if (!info.has_value())
        {
            MIZU_LOG_ERROR("Resource with name {} not found in shader", name);
            resources_valid = false;
            continue;
        }

        if (!std::holds_alternative<ShaderBufferProperty>(info->value))
        {
            MIZU_LOG_ERROR("Resource with name {} is not a Uniform Buffer", name);
            resources_valid = false;
            continue;
        }

        const auto buffer_info = std::get<ShaderBufferProperty>(info->value);

        if (buffer_info.total_size != ubo->get_size())
        {
            MIZU_LOG_ERROR(" Buffer with name {} does not have expected size ({} != {})",
                           name,
                           buffer_info.total_size,
                           ubo->get_size());
            resources_valid = false;
            continue;
        }
    }

    m_baked = resources_valid;

    return resources_valid;
}

void OpenGLResourceGroup::bind(const OpenGLShaderBase& shader) const
{
    if (!m_baked)
    {
        MIZU_LOG_ERROR("Could not bind resources because ResourceGroup was not baked");
        return;
    }

    for (const auto& [name, image] : m_image_resources)
    {
        const auto info = shader.get_property(name);
        MIZU_ASSERT(info.has_value(), "If baked, property should exist");

        const auto& texture_info = std::get<ShaderImageProperty>(info->value);
        if (texture_info.type == ShaderImageProperty::Type::Storage)
        {
            // This path is for storage images (both textures and cubemaps)
            const auto& [internal, _, __] = OpenGLImageResource::get_format_info(image->get_format());
            glBindImageTexture(info->binding_info.binding,
                               image->handle(),
                               0,
                               GL_FALSE,
                               0,
                               GL_WRITE_ONLY,
                               static_cast<GLenum>(internal));
        }
        else
        {
            // This path is for texture types and cubemaps
            const auto location = shader.get_uniform_location(name);
            MIZU_ASSERT(location.has_value(), "Texture uniform location not valid ({})", name);

            const GLenum native_image_type = OpenGLImageResource::get_image_type(image->get_image_type());

            glActiveTexture(GL_TEXTURE0 + info->binding_info.binding);
            glBindTexture(native_image_type, image->handle());

            glUniform1i(*location, static_cast<GLint>(info->binding_info.binding));
        }
    }

    for (const auto& [name, ubo] : m_ubo_resources)
    {
        const auto info = shader.get_property(name);
        MIZU_ASSERT(info.has_value(), "If baked, property should exist");

        // Bind uniform buffer
        glBindBuffer(GL_UNIFORM_BUFFER, ubo->handle());

        const auto binding_point = shader.get_uniform_location(name);
        MIZU_ASSERT(binding_point.has_value(), "Uniform buffer binding point invalid");
        glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(*binding_point), ubo->handle());
    }
}

} // namespace Mizu::OpenGL
