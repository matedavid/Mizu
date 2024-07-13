#include "buffers.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"

namespace Mizu {

glm::vec2 convert_texture_coords(glm::vec2 coords) {
    // Default is top left = {0, 0}

    if (Renderer::get_config().graphics_api == GraphicsAPI::OpenGL) {
        return {1.0 - coords.x, 1.0 - coords.y};
    }

    return coords;
}

std::shared_ptr<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t>& data) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanIndexBuffer>(data);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLIndexBuffer>(data);
    }
}

} // namespace Mizu
