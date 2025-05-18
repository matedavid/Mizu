#pragma once

#include <glad/glad.h>

#include "render_core/rhi/buffer_resource.h"

namespace Mizu::OpenGL
{

class OpenGLBufferResource : public BufferResource
{
  public:
    OpenGLBufferResource(const BufferDescription& desc);
    OpenGLBufferResource(const BufferDescription& desc, const uint8_t* data);

    ~OpenGLBufferResource() override;

    void set_data(const uint8_t* data) const override;

    [[nodiscard]] uint64_t get_size() const override { return m_description.size; }
    BufferUsageBits get_usage() const override { return m_description.usage; }

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    GLenum m_type{};

    BufferDescription m_description;

    static GLenum get_buffer_type(BufferUsageBits usage);
};

} // namespace Mizu::OpenGL