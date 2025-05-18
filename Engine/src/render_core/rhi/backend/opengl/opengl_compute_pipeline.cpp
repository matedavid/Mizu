#include "opengl_compute_pipeline.h"

#include "render_core/resources/buffers.h"
#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/opengl/opengl_shader.h"

#include "utility/assert.h"

namespace Mizu::OpenGL
{

OpenGLComputePipeline::OpenGLComputePipeline(const Description& desc)
{
    m_shader = std::dynamic_pointer_cast<OpenGLComputeShader>(desc.shader);
}

void OpenGLComputePipeline::set_state() const
{
    glUseProgram(m_shader->handle());
}

void OpenGLComputePipeline::push_constant(std::string_view name, uint32_t size, const void* data)
{
    const auto info = m_shader->get_constant(name);
    MIZU_ASSERT(info.has_value(), "Push constant '{}' not found in GraphicsPipeline", name);

    MIZU_ASSERT(info->size == size,
                "Size of provided data and size of push constant do not match ({} != {})",
                size,
                info->size);

    auto constant_it = m_constants.find(std::string{name});
    if (constant_it == m_constants.end())
    {
        const BufferDescription buffer_desc = UniformBuffer::get_buffer_description(size);

        auto ub = std::make_shared<OpenGLBufferResource>(buffer_desc);
        constant_it = m_constants.insert({std::string{name}, ub}).first;
    }

    constant_it->second->set_data(reinterpret_cast<const uint8_t*>(data));

    const auto binding_point = m_shader->get_uniform_location(name);
    MIZU_ASSERT(binding_point.has_value(), "Constant binding point invalid");
    glBindBufferBase(GL_UNIFORM_BUFFER, static_cast<GLuint>(*binding_point), constant_it->second->handle());
}

} // namespace Mizu::OpenGL
