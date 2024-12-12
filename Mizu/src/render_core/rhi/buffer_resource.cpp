#include "buffer_resource.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"

namespace Mizu
{

std::shared_ptr<BufferResource> BufferResource::create(const BufferDescription& desc,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBufferResource>(desc, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLBufferResource>(desc);
    }
}

std::shared_ptr<BufferResource> BufferResource::create(const BufferDescription& desc,
                                                       const uint8_t* data,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBufferResource>(desc, data, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLBufferResource>(desc, data);
    }
}

} // namespace Mizu