#pragma once

#include <filesystem>
#include <memory>
#include <string_view>

#include "render_core/rhi/image_resource.h"

#include "render/core/buffer_utils.h"
#include "mizu_render_module.h"

namespace Mizu
{

namespace ImageUtils
{

MIZU_RENDER_API std::shared_ptr<ImageResource> create_texture2d(const std::filesystem::path& path);
MIZU_RENDER_API std::shared_ptr<ImageResource> create_texture2d(
    glm::uvec2 dimensions,
    ImageFormat format,
    std::span<uint8_t> content,
    std::string name = "");
MIZU_RENDER_API std::shared_ptr<ImageResource> create_texture2d(const ImageDescription& desc, std::span<uint8_t> content);

struct Faces
{
    std::string_view right;
    std::string_view left;
    std::string_view top;
    std::string_view bottom;
    std::string_view front;
    std::string_view back;
};

MIZU_RENDER_API std::shared_ptr<ImageResource> create_cubemap(const Faces& faces, std::string name = "");

} // namespace ImageUtils

} // namespace Mizu