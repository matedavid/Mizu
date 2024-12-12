#include "command_buffer.h"

#include "renderer.h"

#include "render_core/rhi/backend/opengl/opengl_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"

namespace Mizu
{

std::shared_ptr<RenderCommandBuffer> RenderCommandBuffer::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderCommandBuffer>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLRenderCommandBuffer>();
    }
}

std::shared_ptr<ComputeCommandBuffer> ComputeCommandBuffer::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanComputeCommandBuffer>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLComputeCommandBuffer>();
    }
}

} // namespace Mizu
