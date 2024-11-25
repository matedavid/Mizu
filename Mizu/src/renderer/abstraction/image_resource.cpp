#include "image_resource.h"

#include <cmath>

#include "renderer/abstraction/device_memory_allocator.h"
#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_image_resource.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"

namespace Mizu {

std::shared_ptr<ImageResource> ImageResource::create(const ImageDescription& desc,
                                                     const SamplingOptions& sampling,
                                                     std::weak_ptr<IDeviceMemoryAllocator> allocator) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResource>(desc, sampling, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLImageResource>(desc, sampling);
        return nullptr;
    }
}

std::shared_ptr<ImageResource> ImageResource::create(const ImageDescription& desc,
                                                     const SamplingOptions& sampling,
                                                     const std::vector<uint8_t>& content,
                                                     std::weak_ptr<IDeviceMemoryAllocator> allocator) {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResource>(desc, sampling, content, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLImageResource>(desc, sampling, content);
    }
}

bool ImageUtils::is_depth_format(ImageFormat format) {
    return format == ImageFormat::D32_SFLOAT;
}

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height, uint32_t depth) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, std::max(height, depth))))) + 1;
}

} // namespace Mizu