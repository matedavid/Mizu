#include "device_memory_allocator.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/opengl/opengl_device_memory_allocator.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu
{

//
// BaseDeviceMemoryAllocator
//

std::shared_ptr<BaseDeviceMemoryAllocator> BaseDeviceMemoryAllocator::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBaseDeviceMemoryAllocator>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLBaseDeviceMemoryAllocator>();
    }
}

//
// RenderGraphDeviceMemoryAllocator
//

std::shared_ptr<TransientImageResource> TransientImageResource::create(const ImageDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTransientImageResource>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLTransientImageResource>(desc);
    }
}

std::shared_ptr<TransientBufferResource> TransientBufferResource::create(const BufferDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTransientBufferResource>(desc);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLTransientBufferResource>(desc);
    }
}

std::shared_ptr<TransientBufferResource> TransientBufferResource::create(const BufferDescription& desc,
                                                                         const std::vector<uint8_t>& data)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTransientBufferResource>(desc, data);
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLTransientBufferResource>(desc, data);
    }
}

std::shared_ptr<RenderGraphDeviceMemoryAllocator> RenderGraphDeviceMemoryAllocator::create()
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanRenderGraphDeviceMemoryAllocator>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLRenderGraphDeviceMemoryAllocator>();
    }
}

} // namespace Mizu