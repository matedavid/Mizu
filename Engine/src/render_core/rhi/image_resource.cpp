#include "image_resource.h"

#include <cmath>

#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/opengl/opengl_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu
{

std::shared_ptr<ImageResource> ImageResource::create(
    const ImageDescription& desc,
    std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResource>(desc, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLImageResource>(desc);
    }
}

std::shared_ptr<ImageResource> ImageResource::create(
    const ImageDescription& desc,
    const uint8_t* content,
    std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResource>(desc, content, allocator);
    case GraphicsAPI::OpenGL:
        // TODO: Is this good idea? Not passing `allocator` parameter
        return std::make_shared<OpenGL::OpenGLImageResource>(desc, content);
    }
}

bool ImageUtils::is_depth_format(ImageFormat format)
{
    return format == ImageFormat::D32_SFLOAT;
}

uint32_t ImageUtils::get_num_components(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return 1;
    case ImageFormat::RG16_SFLOAT:
    case ImageFormat::RG32_SFLOAT:
        return 2;
    case ImageFormat::RGB32_SFLOAT:
        return 3;
    case ImageFormat::RGBA8_SRGB:
    case ImageFormat::RGBA8_UNORM:
    case ImageFormat::RGBA16_SFLOAT:
    case ImageFormat::RGBA32_SFLOAT:
    case ImageFormat::BGRA8_SRGB:
        return 4;
    case ImageFormat::D32_SFLOAT:
        return 1;
    }
}

uint32_t ImageUtils::get_format_size(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return 1 * sizeof(float);
    case ImageFormat::RG16_SFLOAT:
        return 2 * (sizeof(float) / 2);
    case ImageFormat::RG32_SFLOAT:
        return 2 * sizeof(float);
    case ImageFormat::RGB32_SFLOAT:
        return 3 * sizeof(float);
    case ImageFormat::RGBA8_SRGB:
    case ImageFormat::RGBA8_UNORM:
        return 4 * sizeof(uint8_t);
    case ImageFormat::RGBA16_SFLOAT:
        return 4 * (sizeof(float) / 2);
    case ImageFormat::RGBA32_SFLOAT:
        return 4 * sizeof(float);
    case ImageFormat::BGRA8_SRGB:
        return 4 * sizeof(uint8_t);
    case ImageFormat::D32_SFLOAT:
        return sizeof(float);
    }
}

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height, uint32_t depth)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, std::max(height, depth))))) + 1;
}

glm::uvec2 ImageUtils::compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level)
{
    return glm::max(glm::uvec2(1u), glm::uvec2(original_width, original_height) / (1u << mip_level));
}

} // namespace Mizu