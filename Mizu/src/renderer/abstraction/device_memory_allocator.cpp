#include "device_memory_allocator.h"

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_device_memory_allocator.h"
#include "renderer/abstraction/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu {

//
// BaseDeviceMemoryAllocator
//

std::shared_ptr<BaseDeviceMemoryAllocator> BaseDeviceMemoryAllocator::create() {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBaseDeviceMemoryAllocator>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLBaseDeviceMemoryAllocator>();
    }
}

} // namespace Mizu