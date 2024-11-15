#include "image_resource.h"

#include <cmath>

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"

namespace Mizu {

bool ImageUtils::is_depth_format(ImageFormat format) {
    return format == ImageFormat::D32_SFLOAT;
}

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height, uint32_t depth) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, std::max(height, depth))))) + 1;
}

} // namespace Mizu