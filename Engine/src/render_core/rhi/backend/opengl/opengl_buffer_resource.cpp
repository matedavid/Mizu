#include "opengl_buffer_resource.h"

#include "render_core/rhi/backend/opengl/opengl_context.h"
#include "utility/assert.h"

namespace Mizu::OpenGL
{

OpenGLBufferResource::OpenGLBufferResource(const BufferDescription& desc) : m_description(desc)
{
    glGenBuffers(1, &m_handle);
    m_type = get_buffer_type(m_description.usage);

    if (!m_description.name.empty())
    {
        GL_DEBUG_SET_OBJECT_NAME(GL_BUFFER, m_handle, m_description.name);
    }
}

OpenGLBufferResource::OpenGLBufferResource(const BufferDescription& desc, const uint8_t* data)
    : OpenGLBufferResource(desc)
{
    glBindBuffer(m_type, m_handle);
    glBufferData(m_type, static_cast<GLsizeiptr>(m_description.size), data, GL_STATIC_DRAW);
    glBindBuffer(m_type, 0);
}

OpenGLBufferResource::~OpenGLBufferResource()
{
    glDeleteBuffers(1, &m_handle);
}

void OpenGLBufferResource::set_data(const uint8_t* data) const
{
    glBindBuffer(m_type, m_handle);
    glBufferData(m_type, static_cast<GLsizeiptr>(m_description.size), data, GL_STATIC_DRAW);
    glBindBuffer(m_type, 0);
}

GLenum OpenGLBufferResource::get_buffer_type(BufferUsageBits usage)
{
    if (usage & BufferUsageBits::VertexBuffer)
        return GL_ARRAY_BUFFER;
    else if (usage & BufferUsageBits::IndexBuffer)
        return GL_ELEMENT_ARRAY_BUFFER;
    else if (usage & BufferUsageBits::UniformBuffer)
        return GL_UNIFORM_BUFFER;
    else if (usage & BufferUsageBits::StorageBuffer)
        return GL_SHADER_STORAGE_BUFFER;

    return 0;
}

} // namespace Mizu::OpenGL