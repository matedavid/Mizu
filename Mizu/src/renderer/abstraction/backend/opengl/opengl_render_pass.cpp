#include "opengl_render_pass.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "renderer/abstraction/backend/opengl/opengl_framebuffer.h"
#include "renderer/abstraction/backend/opengl/opengl_texture.h"

namespace Mizu::OpenGL {

OpenGLRenderPass::OpenGLRenderPass(const Description& desc) : m_debug_name(desc.debug_name) {
    m_framebuffer = std::dynamic_pointer_cast<OpenGLFramebuffer>(desc.target_framebuffer);
}

void OpenGLRenderPass::begin() const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer->handle());
    glViewport(
        0, 0, static_cast<GLsizei>(m_framebuffer->get_width()), static_cast<GLsizei>(m_framebuffer->get_height()));

    uint32_t color_buffer_idx = 0;
    for (const auto& attachment : m_framebuffer->get_attachments()) {
        if (ImageUtils::is_depth_format(attachment.image->get_format())) {
            glClearBufferfv(GL_DEPTH, 0, glm::value_ptr(attachment.clear_value));
        } else {
            glClearBufferfv(GL_COLOR, static_cast<GLint>(color_buffer_idx++), glm::value_ptr(attachment.clear_value));
        }
    }
}

void OpenGLRenderPass::end() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mizu::OpenGL
