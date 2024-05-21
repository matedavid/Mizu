#include "vulkan_texture.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"

namespace Mizu::Vulkan {

VulkanTexture2D::VulkanTexture2D(const ImageDescription& desc) : m_description(desc) {
    VulkanImage::Description description{};
    description.width = m_description.width;
    description.height = m_description.height;
    description.format = m_description.format;
    description.depth = 1;
    description.type = VK_IMAGE_TYPE_2D;
    description.num_layers = 1;
    description.mip_levels =
        m_description.generate_mips ? ImageUtils::compute_num_mips(m_description.width, m_description.height) : 1;

    // Usage
    {
        description.usage = VK_IMAGE_USAGE_SAMPLED_BIT; // TODO: Bad default?

        const bool usage_attachment = m_description.usage & ImageUsageBits::Attachment;
        if (usage_attachment && ImageUtils::is_depth_format(m_description.format))
            description.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else if (usage_attachment)
            description.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (m_description.usage & ImageUsageBits::Storage)
            description.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    m_image = std::make_shared<VulkanImage>(description);

    // Create sampler
    // TODO: Still a lot of parameters need to be configured
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = get_filter(m_description.sampling_options.magnification_filter);
    sampler_create_info.minFilter = get_filter(m_description.sampling_options.minification_filter);
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU = get_sampler_address_mode(m_description.sampling_options.address_mode_u);
    sampler_create_info.addressModeV = get_sampler_address_mode(m_description.sampling_options.address_mode_v);
    sampler_create_info.addressModeW = get_sampler_address_mode(m_description.sampling_options.address_mode_w);
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 0.0f;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 0.0f;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(VulkanContext.device->handle(), &sampler_create_info, nullptr, &m_sampler));
}

VulkanTexture2D::~VulkanTexture2D() {
    vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
}

VkFilter VulkanTexture2D::get_filter(ImageFilter filter) {
    switch (filter) {
    case ImageFilter::Nearest:
        return VK_FILTER_NEAREST;
    case ImageFilter::Linear:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode VulkanTexture2D::get_sampler_address_mode(ImageAddressMode mode) {
    switch (mode) {
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

} // namespace Mizu::Vulkan
