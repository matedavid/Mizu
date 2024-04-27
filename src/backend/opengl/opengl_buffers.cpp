#include "opengl_buffers.h"

namespace Mizu::OpenGL {

//
// OpenGLUniformBuffer
//

OpenGLUniformBuffer::OpenGLUniformBuffer(uint32_t size) : m_size(size) {
    glGenBuffers(1, &m_handle);
    glBindBuffer(GL_UNIFORM_BUFFER, m_handle);
    glBufferData(GL_UNIFORM_BUFFER, m_size, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

OpenGLUniformBuffer::~OpenGLUniformBuffer() {
    glDeleteBuffers(1, &m_handle);
}

void OpenGLUniformBuffer::set_data(const void* data) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_handle);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, m_size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void OpenGLUniformBuffer::set_data(const void* data, uint32_t size, uint32_t offset_bytes) {
    glBindBuffer(GL_UNIFORM_BUFFER, m_handle);
    glBufferSubData(GL_UNIFORM_BUFFER, offset_bytes, size, data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

} // namespace Mizu::OpenGL
