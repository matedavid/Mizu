#include "opengl_buffers.h"

namespace Mizu::OpenGL {

//
// OpenGLVertexBuffer
//

OpenGLVertexBuffer::OpenGLVertexBuffer(const void* data,
                                       uint32_t count,
                                       uint32_t size,
                                       const std::vector<Layout>& layout)
      : m_count(count) {
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vbo);

    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, size, data, GL_STATIC_DRAW);

    uint32_t generic_stride = 0;
    for (const auto& element : layout) {
        generic_stride += element.count * get_type_size(element.type);
    }

    uint32_t stride = 0;
    for (uint32_t i = 0; i < layout.size(); ++i) {
        auto element = layout[i];
        glVertexAttribPointer(i,
                              static_cast<GLint>(element.count),
                              get_opengl_type(element.type),
                              element.normalized ? GL_TRUE : GL_FALSE,
                              static_cast<GLsizei>(generic_stride),
                              reinterpret_cast<const void*>(stride));
        glEnableVertexAttribArray(i);

        stride += element.count * get_type_size(element.type);
    }

    glBindVertexArray(0);
}

OpenGLVertexBuffer::~OpenGLVertexBuffer() {
    glDeleteBuffers(1, &m_vbo);
    glDeleteVertexArrays(1, &m_vao);
}

void OpenGLVertexBuffer::bind() const {
    glBindVertexArray(m_vao);
}

uint32_t OpenGLVertexBuffer::get_type_size(Layout::Type type) {
    using Type = Layout::Type;

    switch (type) {
    case Type::Float:
        return sizeof(float);
    case Type::UnsignedInt:
        return sizeof(uint32_t);
    }
}

GLenum OpenGLVertexBuffer::get_opengl_type(Layout::Type type) {
    using Type = VertexBuffer::Layout::Type;

    switch (type) {
    case Type::Float:
        return GL_FLOAT;
    case Type::UnsignedInt:
        return GL_UNSIGNED_INT;
    }
}

//
// OpenGLIndexBuffer
//

OpenGLIndexBuffer::OpenGLIndexBuffer(const std::vector<uint32_t>& data) {
    m_count = static_cast<uint32_t>(data.size());

    glGenBuffers(1, &m_handle);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);

    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(data.size() * sizeof(uint32_t)), data.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

OpenGLIndexBuffer::~OpenGLIndexBuffer() {
    glDeleteBuffers(1, &m_handle);
}

void OpenGLIndexBuffer::bind() const {
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_handle);
}

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
