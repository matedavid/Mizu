#pragma once

#include <memory>
#include <string_view>

#include "render_core/rhi/image_resource.h"

#include "mizu_render_module.h"

namespace Mizu
{

namespace ImageUtils
{

MIZU_RENDER_API std::shared_ptr<ImageResource> create_texture2d(
    glm::uvec2 dimensions,
    ImageFormat format,
    std::span<uint8_t> content,
    std::string name = "");
MIZU_RENDER_API std::shared_ptr<ImageResource> create_texture2d(
    const ImageDescription& desc,
    std::span<uint8_t> content);

} // namespace ImageUtils

} // namespace Mizu