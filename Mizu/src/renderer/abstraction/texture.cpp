#include "texture.h"

#include "renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_texture.h"
#include "renderer/abstraction/backend/vulkan/vulkan_texture.h"

namespace Mizu {

std::shared_ptr<Texture2D> Texture2D::create(const ImageDescription& desc) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTexture2D>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLTexture2D>(desc);
    }
}

std::shared_ptr<Texture2D> Texture2D::create(const std::filesystem::path& path, SamplingOptions options) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTexture2D>(path, options);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLTexture2D>(path, options);
    }
}

} // namespace Mizu
