#include "framebuffer.h"

#include "renderer.h"

#include "render_core/rhi/backend/opengl/opengl_framebuffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_framebuffer.h"

namespace Mizu
{

std::shared_ptr<Framebuffer> Framebuffer::create(const Description& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanFramebuffer>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLFramebuffer>(desc);
    }
}

} // namespace Mizu
