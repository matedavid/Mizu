#pragma once

#include <glad/glad.h>
#include <vector>

#include "renderer/abstraction/framebuffer.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLImageResource;

class OpenGLFramebuffer : public Framebuffer {
  public:
    explicit OpenGLFramebuffer(const Description& desc);
    ~OpenGLFramebuffer() override;

    [[nodiscard]] std::vector<Attachment> get_attachments() const override { return m_description.attachments; }

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    Description m_description;

    std::unique_ptr<OpenGLImageResource> m_color_attachment;
};

} // namespace Mizu::OpenGL