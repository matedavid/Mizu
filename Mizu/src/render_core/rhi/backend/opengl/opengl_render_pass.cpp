#include "opengl_render_pass.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include "render_core/resources/texture.h"

#include "render_core/rhi/backend/opengl/opengl_context.h"
#include "render_core/rhi/backend/opengl/opengl_framebuffer.h"
#include "render_core/rhi/backend/opengl/opengl_image_resource.h"

namespace Mizu::OpenGL
{

OpenGLRenderPass::OpenGLRenderPass(const Description& desc)
{
    m_framebuffer = std::dynamic_pointer_cast<OpenGLFramebuffer>(desc.target_framebuffer);
}

void OpenGLRenderPass::begin() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer->handle());
    glViewport(
        0, 0, static_cast<GLsizei>(m_framebuffer->get_width()), static_cast<GLsizei>(m_framebuffer->get_height()));

    uint32_t color_buffer_idx = 0;
    for (const auto& attachment : m_framebuffer->get_attachments())
    {
        if (attachment.load_operation == LoadOperation::Clear)
        {
            const auto& native_resource =
                std::dynamic_pointer_cast<OpenGLImageResource>(attachment.image->get_resource());
            if (ImageUtils::is_depth_format(attachment.image->get_resource()->get_format()))
            {
                glClearTexImage(
                    native_resource->handle(), 0, GL_DEPTH_COMPONENT, GL_FLOAT, glm::value_ptr(attachment.clear_value));
            }
            else
            {
                glClearTexImage(native_resource->handle(), 0, GL_RGB, GL_FLOAT, glm::value_ptr(attachment.clear_value));
            }
        }
    }
}

void OpenGLRenderPass::end() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

std::shared_ptr<Framebuffer> OpenGLRenderPass::get_framebuffer() const
{
    return std::dynamic_pointer_cast<Framebuffer>(m_framebuffer);
}

} // namespace Mizu::OpenGL
