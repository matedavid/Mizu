#include "vulkan_texture.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"

namespace Mizu::Vulkan {

VulkanTexture2D::VulkanTexture2D(const ImageDescription& desc) {
    VulkanImage::Description description{};
    description.width = desc.width;
    description.height = desc.height;
    description.depth = 1;
    description.format = desc.format;
    description.type = VK_IMAGE_TYPE_2D;
    description.usage = desc.usage;
    description.flags = 0;
    description.num_mips = desc.generate_mips ? ImageUtils::compute_num_mips(desc.width, desc.height) : 1;
    description.num_layers = 1;

    init_resources(description, desc.sampling_options);
}

VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height, VkImage image, VkImageView view, bool owning)
      : VulkanImage(image, view, owning) {
    m_description.width = width;
    m_description.height = height;
}

/*
VulkanTexture2D::VulkanTexture2D(const ImageDescription& desc) : m_description(desc) {
    assert(m_description.usage != ImageUsageBits::None && "Texture2D usage can't be None");

    VulkanImage::Description description{};
    description.width = m_description.width;
    description.height = m_description.height;
    description.depth = 1;
    description.format = m_description.format;
    description.type = VK_IMAGE_TYPE_2D;
    description.num_layers = 1;
    description.mip_levels =
        m_description.generate_mips ? ImageUtils::compute_num_mips(m_description.width, m_description.height) : 1;

    // Usage
    {
        description.usage = 0;

        const bool usage_attachment = m_description.usage & ImageUsageBits::Attachment;
        if (usage_attachment && ImageUtils::is_depth_format(m_description.format))
            description.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else if (usage_attachment)
            description.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (m_description.usage & ImageUsageBits::Sampled)
            description.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (m_description.usage & ImageUsageBits::Storage)
            description.usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    m_image = std::make_shared<VulkanImage>(description);

    // Create sampler
    // TODO: Still a lot of parameters need to be configured
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = VulkanImage::get_vulkan_filter(m_description.sampling_options.magnification_filter);
    sampler_create_info.minFilter = VulkanImage::get_vulkan_filter(m_description.sampling_options.minification_filter);
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU =
        VulkanImage::get_vulkan_sampler_address_mode(m_description.sampling_options.address_mode_u);
    sampler_create_info.addressModeV =
        VulkanImage::get_vulkan_sampler_address_mode(m_description.sampling_options.address_mode_v);
    sampler_create_info.addressModeW =
        VulkanImage::get_vulkan_sampler_address_mode(m_description.sampling_options.address_mode_w);
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

VulkanTexture2D::VulkanTexture2D(const ImageDescription& desc, std::shared_ptr<VulkanImage> image)
      : m_image(std::move(image)), m_description(desc) {
    assert(m_image != nullptr && "Image provided can't be nullptr");

    // TODO: Should also create sampler?
    // For the moment no because the usage is very specific, in the Swapchain only
}

VulkanTexture2D::~VulkanTexture2D() {
    vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
}
*/

} // namespace Mizu::Vulkan
