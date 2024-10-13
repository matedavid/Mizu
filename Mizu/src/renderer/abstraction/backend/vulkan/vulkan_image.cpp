#include "vulkan_image.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/texture.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

void VulkanImage::init_resources(const Description& desc, const SamplingOptions& sampling_options) {
    m_description = desc;
    m_sampling_options = sampling_options;

    create_image();
    create_image_view();
    create_sampler();
}

VulkanImage::VulkanImage(VkImage image, VkImageView view, bool owning)
      : m_image(image), m_image_view(view), m_owns_resources(owning) {
    assert(m_image != VK_NULL_HANDLE && "Image can't be VK_NULL_HANDLE");
    assert(m_image_view != VK_NULL_HANDLE && "ImageView can't be VK_NULL_HANDLE");
}

VulkanImage::~VulkanImage() {
    if (m_owns_resources) {
        vkDestroyImage(VulkanContext.device->handle(), m_image, nullptr);
        vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);

        vkDestroyImageView(VulkanContext.device->handle(), m_image_view, nullptr);

        vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
    }
}

VkFormat VulkanImage::get_image_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    }
}

VkImageViewType VulkanImage::get_image_view_type(VkImageType type) {
    switch (type) {
    case VK_IMAGE_TYPE_1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case VK_IMAGE_TYPE_2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case VK_IMAGE_TYPE_3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    default:
        MIZU_UNREACHABLE("ImageView type not valid");
        break;
    }
}

VkImageLayout VulkanImage::get_vulkan_image_resource_state(ImageResourceState state) {
    switch (state) {
    case ImageResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageResourceState::General:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ImageResourceState::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ImageResourceState::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageResourceState::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageResourceState::DepthStencilAttachment:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }
}

VkFilter VulkanImage::get_vulkan_filter(ImageFilter filter) {
    switch (filter) {
    case ImageFilter::Nearest:
        return VK_FILTER_NEAREST;
    case ImageFilter::Linear:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode VulkanImage::get_vulkan_sampler_address_mode(ImageAddressMode mode) {
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

void VulkanImage::create_image() {
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = m_description.type;
    image_create_info.format = get_image_format(m_description.format);
    image_create_info.extent =
        VkExtent3D{.width = m_description.width, .height = m_description.height, .depth = m_description.depth};
    image_create_info.mipLevels = m_description.num_mips;
    image_create_info.arrayLayers = m_description.num_layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = nullptr;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.flags = m_description.flags;

    // Usage
    {
        image_create_info.usage = 0;

        const bool usage_attachment = m_description.usage & ImageUsageBits::Attachment;
        if (usage_attachment && ImageUtils::is_depth_format(m_description.format))
            image_create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        else if (usage_attachment)
            image_create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        if (m_description.usage & ImageUsageBits::Sampled)
            image_create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

        if (m_description.usage & ImageUsageBits::Storage)
            image_create_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

        if (m_description.usage & ImageUsageBits::TransferDst)
            image_create_info.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    m_current_state = ImageResourceState::Undefined;

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_image));

    // Allocate image
    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), m_image, &memory_requirements);

    const auto memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    assert(memory_type_index.has_value() && "No suitable memory to allocate image");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));

    VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), m_image, m_memory, 0));
}

void VulkanImage::create_image_view() {
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_image;
    view_create_info.viewType = (m_description.flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
                                    ? VK_IMAGE_VIEW_TYPE_CUBE
                                    : get_image_view_type(m_description.type);
    view_create_info.format = get_image_format(m_description.format);

    if (ImageUtils::is_depth_format(m_description.format))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = m_description.num_mips;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = m_description.num_layers;

    // TODO: Create mip image views

    VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &m_image_view));
}

void VulkanImage::create_sampler() {
    // TODO: Still a lot of parameters need to be configured
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = get_vulkan_filter(m_sampling_options.magnification_filter);
    sampler_create_info.minFilter = get_vulkan_filter(m_sampling_options.minification_filter);
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_u);
    sampler_create_info.addressModeV = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_v);
    sampler_create_info.addressModeW = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_w);
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

} // namespace Mizu::Vulkan
