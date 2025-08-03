#include "vulkan_sampler_state.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan
{

VulkanSamplerState::VulkanSamplerState(SamplingOptions options)
{
    // TODO: Still a lot of parameters need to be configured
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = get_vulkan_filter(options.magnification_filter);
    sampler_create_info.minFilter = get_vulkan_filter(options.minification_filter);
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU = get_vulkan_sampler_address_mode(options.address_mode_u);
    sampler_create_info.addressModeV = get_vulkan_sampler_address_mode(options.address_mode_v);
    sampler_create_info.addressModeW = get_vulkan_sampler_address_mode(options.address_mode_w);
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 0.0f;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 1.0f;
    sampler_create_info.borderColor = get_vulkan_border_color(options.border_color);
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(VulkanContext.device->handle(), &sampler_create_info, nullptr, &m_sampler));
}

VulkanSamplerState::~VulkanSamplerState()
{
    vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
}

VkFilter VulkanSamplerState::get_vulkan_filter(ImageFilter filter)
{
    switch (filter)
    {
    case ImageFilter::Nearest:
        return VK_FILTER_NEAREST;
    case ImageFilter::Linear:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode VulkanSamplerState::get_vulkan_sampler_address_mode(ImageAddressMode mode)
{
    switch (mode)
    {
    case ImageAddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case ImageAddressMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case ImageAddressMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case ImageAddressMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
}

VkBorderColor VulkanSamplerState::get_vulkan_border_color(BorderColor color)
{
    switch (color)
    {
    case BorderColor::FloatTransparentBlack:
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    case BorderColor::IntTransparentBlack:
        return VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
    case BorderColor::FloatOpaqueBlack:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    case BorderColor::IntOpaqueBlack:
        return VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    case BorderColor::FloatOpaqueWhite:
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    case BorderColor::IntOpaqueWhite:
        return VK_BORDER_COLOR_INT_OPAQUE_WHITE;
    }
}

} // namespace Mizu::Vulkan