#include "synchronization.h"

#include "renderer.h"

#include "render_core/rhi/backend/opengl/opengl_synchronization.h"
#include "render_core/rhi/backend/vulkan/vulkan_synchronization.h"

namespace Mizu
{

std::shared_ptr<Fence> Fence::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanFence>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLFence>();
    }
}

std::shared_ptr<Semaphore> Semaphore::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSemaphore>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLSemaphore>();
    }
}

} // namespace Mizu
