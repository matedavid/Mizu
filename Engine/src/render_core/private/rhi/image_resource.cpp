#include "render_core/rhi/image_resource.h"

#include <algorithm>
#include <cmath>

#include "base/debug/assert.h"

namespace Mizu
{

bool is_depth_format(ImageFormat format)
{
    return format == ImageFormat::D32_SFLOAT;
}

bool is_srgb_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R8G8B8A8_SRGB:
    case ImageFormat::B8G8R8A8_SRGB:
        return true;

    case ImageFormat::R32_SFLOAT:
    case ImageFormat::R16G16_SFLOAT:
    case ImageFormat::R32G32_SFLOAT:
    case ImageFormat::R32G32B32_SFLOAT:
    case ImageFormat::R8G8B8A8_UNORM:
    case ImageFormat::R16G16B16A16_SFLOAT:
    case ImageFormat::R32G32B32A32_SFLOAT:
    case ImageFormat::B8G8R8A8_UNORM:
    case ImageFormat::D32_SFLOAT:
        return false;
    }
}

uint32_t get_image_format_num_components(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return 1;
    case ImageFormat::R16G16_SFLOAT:
    case ImageFormat::R32G32_SFLOAT:
        return 2;
    case ImageFormat::R32G32B32_SFLOAT:
        return 3;
    case ImageFormat::R8G8B8A8_SRGB:
    case ImageFormat::R8G8B8A8_UNORM:
    case ImageFormat::R16G16B16A16_SFLOAT:
    case ImageFormat::R32G32B32A32_SFLOAT:
    case ImageFormat::B8G8R8A8_SRGB:
    case ImageFormat::B8G8R8A8_UNORM:
        return 4;
    case ImageFormat::D32_SFLOAT:
        return 1;
    }
}

uint32_t get_image_format_size(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return 1 * sizeof(float);
    case ImageFormat::R16G16_SFLOAT:
        return 2 * (sizeof(float) / 2);
    case ImageFormat::R32G32_SFLOAT:
        return 2 * sizeof(float);
    case ImageFormat::R32G32B32_SFLOAT:
        return 3 * sizeof(float);
    case ImageFormat::R8G8B8A8_SRGB:
    case ImageFormat::R8G8B8A8_UNORM:
        return 4 * sizeof(uint8_t);
    case ImageFormat::R16G16B16A16_SFLOAT:
        return 4 * (sizeof(float) / 2);
    case ImageFormat::R32G32B32A32_SFLOAT:
        return 4 * sizeof(float);
    case ImageFormat::B8G8R8A8_SRGB:
    case ImageFormat::B8G8R8A8_UNORM:
        return 4 * sizeof(uint8_t);
    case ImageFormat::D32_SFLOAT:
        return sizeof(float);
    }
}

uint32_t compute_num_mips(uint32_t width, uint32_t height, uint32_t depth)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, std::max(height, depth))))) + 1;
}

glm::uvec2 compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level)
{
    return glm::max(glm::uvec2(1u), glm::uvec2(original_width, original_height) / (1u << mip_level));
}

glm::uvec3 get_sparse_mip_tile_count(
    const ImageDescription& desc,
    const SparseImageMemoryRequirements& reqs,
    uint32_t mip_level)
{
    MIZU_ASSERT(
        mip_level < reqs.mip_tail_first_mip,
        "Mip {} is part of the mip tail (first mip tail = {}), it has no regular tiles",
        mip_level,
        reqs.mip_tail_first_mip);
    MIZU_ASSERT(
        reqs.tile_extent.x != 0 && reqs.tile_extent.y != 0 && reqs.tile_extent.z != 0, "Invalid sparse tile extent");

    const glm::uvec2 mip_size = compute_mip_size(desc.width, desc.height, mip_level);
    const uint32_t mip_depth = desc.type == ImageType::Image3D ? std::max(1u, desc.depth >> mip_level) : 1u;

    return (glm::uvec3{mip_size, mip_depth} + reqs.tile_extent - 1u) / reqs.tile_extent;
}

} // namespace Mizu