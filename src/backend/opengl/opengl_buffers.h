#pragma once

#include <glad/glad.h>
#include <vector>

#include "buffers.h"

namespace Mizu::OpenGL {

class OpenGLVertexBuffer : public VertexBuffer {
  public:
    OpenGLVertexBuffer(const void* data, uint32_t count, uint32_t size, const std::vector<Layout>& layout);
    ~OpenGLVertexBuffer() override;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] uint32_t count() const override { return m_count; }

  private:
    GLuint m_vao{};
    GLuint m_vbo{};
    uint32_t m_count{};

    [[nodiscard]] static uint32_t get_type_size(Layout::Type type);
    [[nodiscard]] static GLenum get_opengl_type(Layout::Type type);
};

class OpenGLIndexBuffer : public IndexBuffer {
  public:
    explicit OpenGLIndexBuffer(const std::vector<uint32_t>& data);
    ~OpenGLIndexBuffer() override;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] uint32_t count() const override { return m_count; }

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    uint32_t m_count{};
};

class OpenGLUniformBuffer : public UniformBuffer {
  public:
    explicit OpenGLUniformBuffer(uint32_t size);
    ~OpenGLUniformBuffer() override;

    void set_data(const void* data) override;
    void set_data(const void* data, uint32_t size, uint32_t offset_bytes) override;

    [[nodiscard]] uint32_t size() const override { return m_size; }

    [[nodiscard]] uint32_t handle() const { return m_handle; }

  private:
    uint32_t m_handle{};
    uint32_t m_size;
};

} // namespace Mizu::OpenGL
