#pragma once

#include <glad/glad.h>
#include <vector>

#include "render_core/rhi/framebuffer.h"

namespace Mizu::OpenGL
{

// Forward declarations
class OpenGLImageResource;

class OpenGLFramebuffer : public Framebuffer
{
  public:
    explicit OpenGLFramebuffer(Description desc);
    ~OpenGLFramebuffer() override;

    std::vector<Attachment> get_attachments() const override { return m_description.attachments; }

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }

    GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    Description m_description;

    std::unique_ptr<OpenGLImageResource> m_color_attachment;
};

} // namespace Mizu::OpenGL