#include "cubemap.h"

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_cubemap.h"
#include "renderer/abstraction/backend/vulkan/vulkan_cubemap.h"

namespace Mizu {

std::shared_ptr<Cubemap> Cubemap::create(const ImageDescription& desc) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanCubemap>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLCubemap>(desc);
    }
}

std::shared_ptr<Cubemap> Cubemap::create(const Faces& faces) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanCubemap>(faces);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLCubemap>(faces);
    }
}

} // namespace Mizu
