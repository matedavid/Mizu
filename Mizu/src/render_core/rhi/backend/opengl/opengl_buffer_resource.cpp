#include "opengl_buffer_resource.h"

#include "utility/assert.h"

namespace Mizu::OpenGL
{

OpenGLBufferResource::OpenGLBufferResource(const BufferDescription& desc) : m_description(desc)
{
    glGenBuffers(1, &m_handle);
    m_type = get_buffer_type(m_description.type);
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

GLenum OpenGLBufferResource::get_buffer_type(BufferType type)
{
    switch (type)
    {
    case BufferType::VertexBuffer:
        return GL_ARRAY_BUFFER;
    case BufferType::IndexBuffer:
        return GL_ELEMENT_ARRAY_BUFFER;
    case BufferType::UniformBuffer:
        return GL_UNIFORM_BUFFER;
    case BufferType::StorageBuffer:
        MIZU_UNREACHABLE("Unimplemented");
        break;
    case BufferType::Staging:
        return 0;
    }
}

} // namespace Mizu::OpenGL