#include "synchronization.h"

#include "renderer.h"

#include "backend/opengl/opengl_synchronization.h"
#include "backend/vulkan/vulkan_synchronization.h"

namespace Mizu {

std::shared_ptr<Fence> Fence::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanFence>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLFence>();
    }
}

std::shared_ptr<Semaphore> Semaphore::create() {
    switch (get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanSemaphore>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLSemaphore>();
    }
}

} // namespace Mizu
