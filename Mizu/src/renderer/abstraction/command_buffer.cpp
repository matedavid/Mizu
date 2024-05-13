#include "command_buffer.h"

#include "renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"

namespace Mizu {

std::shared_ptr<RenderCommandBuffer> RenderCommandBuffer::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderCommandBuffer>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLRenderCommandBuffer>();
    }
}

std::shared_ptr<ComputeCommandBuffer> ComputeCommandBuffer::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputeCommandBuffer>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLComputeCommandBuffer>();
    }
}

} // namespace Mizu
