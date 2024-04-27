#include "opengl_render_pass.h"

#include <glad/glad.h>

#include "backend/opengl/opengl_framebuffer.h"

namespace Mizu::OpenGL {

OpenGLRenderPass::OpenGLRenderPass(const Description& desc) : m_debug_name(desc.debug_name) {
    m_framebuffer = std::dynamic_pointer_cast<OpenGLFramebuffer>(desc.target_framebuffer);
}

void OpenGLRenderPass::begin([[maybe_unused]] const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer->handle());
}

void OpenGLRenderPass::end([[maybe_unused]] const std::shared_ptr<ICommandBuffer>& command_buffer) const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

} // namespace Mizu::OpenGL
