#pragma once

#include <glad/glad.h>

#include "renderer/abstraction/buffer_resource.h"

namespace Mizu::OpenGL {

class OpenGLBufferResource : public BufferResource {
  public:
    OpenGLBufferResource(const BufferDescription& desc);
    OpenGLBufferResource(const BufferDescription& desc, const uint8_t* data);

    ~OpenGLBufferResource() override;

    void set_data(const uint8_t* data) const override;

    [[nodiscard]] size_t get_size() const override { return m_description.size; }
    [[nodiscard]] BufferType get_type() const override { return m_description.type; }

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    GLenum m_type{};

    BufferDescription m_description;

    [[nodiscard]] static GLenum get_buffer_type(BufferType type);
};

} // namespace Mizu::OpenGL