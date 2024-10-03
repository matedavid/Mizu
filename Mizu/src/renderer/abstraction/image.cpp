#include "image.h"

#include <cmath>

namespace Mizu {

bool ImageUtils::is_depth_format(ImageFormat format) {
    return format == ImageFormat::D32_SFLOAT;
}

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

}