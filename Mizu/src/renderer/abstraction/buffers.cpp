#include "buffers.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffers.h"

namespace Mizu {

std::shared_ptr<IndexBuffer> IndexBuffer::create(const std::vector<uint32_t>& data) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanIndexBuffer>(data);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLIndexBuffer>(data);
    }
}

} // namespace Mizu