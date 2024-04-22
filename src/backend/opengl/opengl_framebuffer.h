#pragma once

#include <glad/glad.h>
#include <vector>

#include "framebuffer.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLTexture2D;

class OpenGLFramebuffer : public Framebuffer {
  public:
    OpenGLFramebuffer(const Description& desc);
    ~OpenGLFramebuffer() override;

    [[nodiscard]] std::vector<Attachment> get_attachments() const override { return m_description.attachments; }

  private:
    GLuint m_handle{};
    Description m_description;

    std::unique_ptr<OpenGLTexture2D> m_color_attachment;
};

} // namespace Mizu::OpenGL