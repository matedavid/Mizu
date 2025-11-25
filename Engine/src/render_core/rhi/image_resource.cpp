#include "image_resource.h"

#include <cmath>

#include "base/debug/assert.h"

#include "render_core/rhi/renderer.h"

#include "render_core/rhi/backend/directx12/dx12_image_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu
{

std::shared_ptr<ImageResource> ImageResource::create(const ImageDescription& desc)
{
    switch (Renderer::get_config().graphics_api)
    {
    case GraphicsApi::DirectX12:
        return std::make_shared<Dx12::Dx12ImageResource>(desc);
    case GraphicsApi::Vulkan:
        return std::make_shared<Vulkan::VulkanImageResource>(desc);
    }
}

#if MIZU_DEBUG

std::string_view image_resource_state_to_string(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return "Undefined";
    case ImageResourceState::UnorderedAccess:
        return "UnorderedAccess";
    case ImageResourceState::TransferDst:
        return "TransferDst";
    case ImageResourceState::ShaderReadOnly:
        return "ShaderReadOnly";
    case ImageResourceState::ColorAttachment:
        return "ColorAttachment";
    case ImageResourceState::DepthStencilAttachment:
        return "DepthStencilAttachment";
    case ImageResourceState::Present:
        return "Present";
    }
}

#endif

//
// ImageUtils
//

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

uint32_t ImageUtils::get_format_size(ImageFormat format)
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

uint32_t ImageUtils::compute_num_mips(uint32_t width, uint32_t height, uint32_t depth)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, std::max(height, depth))))) + 1;
}

glm::uvec2 ImageUtils::compute_mip_size(uint32_t original_width, uint32_t original_height, uint32_t mip_level)
{
    return glm::max(glm::uvec2(1u), glm::uvec2(original_width, original_height) / (1u << mip_level));
}

} // namespace Mizu