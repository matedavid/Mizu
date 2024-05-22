#include "opengl_framebuffer.h"

#include "utility/logging.h"

#include "renderer/abstraction/backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

static std::string get_framebuffer_status_string(GLenum status) {
    switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
        return "Framebuffer is complete.";
    case GL_FRAMEBUFFER_UNDEFINED:
        return "Framebuffer is undefined.";
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        return "Framebuffer has incomplete attachment(s).";
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        return "Framebuffer is missing required attachments.";
    case GL_FRAMEBUFFER_UNSUPPORTED:
        return "Framebuffer format is unsupported.";
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        return "Framebuffer has incomplete multisample configuration.";
    // Add more cases as needed
    default:
        return "Unknown framebuffer status.";
    }
}

OpenGLFramebuffer::OpenGLFramebuffer(const Description& desc) : m_description(desc) {
    glGenFramebuffers(1, &m_handle);
    glBindFramebuffer(GL_FRAMEBUFFER, m_handle);

    uint32_t num_color_attachments = 0, num_depth_attachments = 0;

    for (const auto& attachment : m_description.attachments) {
        const auto& native_image = std::dynamic_pointer_cast<OpenGLTexture2D>(attachment.image);

        if (native_image->get_width() != m_description.width || native_image->get_height() != m_description.height) {
            MIZU_LOG_WARNING("Some attachments in framebuffer don't match in width and height with framebuffer");
        }

        if (ImageUtils::is_depth_format(native_image->get_format())) {
            if (num_depth_attachments == 1) {
                MIZU_LOG_ERROR("Can't bind more than one depth/stencil attachments in framebuffer");
                continue;
            }

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_COMPONENT, GL_TEXTURE_2D, native_image->handle(), 0);

            num_depth_attachments++;
        } else {
            glFramebufferTexture2D(
                GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + num_color_attachments, GL_TEXTURE_2D, native_image->handle(), 0);

            num_color_attachments++;
        }
    }

    // OpenGL Framebuffer requires at least one color attachment. Add one if no color attachment has been provided.
    if (num_color_attachments == 0) {
        ImageDescription tex_desc{};
        tex_desc.width = m_description.width;
        tex_desc.height = m_description.height;
        tex_desc.format = ImageFormat::RGBA8_SRGB;
        tex_desc.usage = ImageUsageBits::Attachment;

        m_color_attachment = std::make_unique<OpenGLTexture2D>(tex_desc);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color_attachment->handle(), 0);
    }

    const auto status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        MIZU_LOG_ERROR("Failed to create framebuffer: {}", get_framebuffer_status_string(status));
        assert(false);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

OpenGLFramebuffer::~OpenGLFramebuffer() {
    glDeleteFramebuffers(1, &m_handle);
}

} // namespace Mizu::OpenGL