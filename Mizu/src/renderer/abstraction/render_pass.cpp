#include "render_pass.h"

#include "renderer.h"

#include "backend/opengl/opengl_render_pass.h"
#include "backend/vulkan/vulkan_render_pass.h"

namespace Mizu {

std::shared_ptr<RenderPass> RenderPass::create(const Description& desc) {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderPass>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLRenderPass>(desc);
    }
}

} // namespace Mizu
