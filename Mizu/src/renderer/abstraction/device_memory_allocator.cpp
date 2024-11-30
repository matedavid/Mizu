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

//
// RenderGraphDeviceMemoryAllocator stuff
//

std::shared_ptr<TransientImageResource> TransientImageResource::create(const ImageDescription& desc,
                                                                       const SamplingOptions& sampling) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTransientImageResource>(desc, sampling);
    case GraphicsAPI::OpenGL:
        // return std::make_shared<OpenGL::OpenGLBaseDeviceMemoryAllocator>();
        return nullptr;
    }
}

std::shared_ptr<RenderGraphDeviceMemoryAllocator> RenderGraphDeviceMemoryAllocator::create() {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderGraphDeviceMemoryAllocator>();
    case GraphicsAPI::OpenGL:
        // return std::make_shared<OpenGL::OpenGLBaseDeviceMemoryAllocator>();
        return nullptr;
    }
}

} // namespace Mizu