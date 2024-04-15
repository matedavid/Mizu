#include "texture.h"

#include <cmath>

#include "renderer.h"

#include "backend/vulkan/vulkan_texture.h"

namespace Mizu {

std::shared_ptr<Texture2D> Texture2D::create(const ImageDescription& desc) {
    switch (get_config().graphics_api) {
    default:
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanTexture2D>(desc);
    }
}

bool ImageUtils::is_depth_format(ImageFormat format) {
    return format == ImageFormat::D32_SFLOAT;
}

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

} // namespace Mizu
