#include "vulkan_image.h"

#include "backend/vulkan/vk_core.h"
#include "backend/vulkan/vulkan_context.h"
#include "backend/vulkan/vulkan_device.h"

namespace Mizu::Vulkan {

VulkanImage::VulkanImage(const Description& desc) : m_description(desc) {
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = m_description.type;
    image_create_info.format = get_image_format(m_description.format);
    image_create_info.extent =
        VkExtent3D{.width = m_description.width, .height = m_description.height, .depth = m_description.depth};
    image_create_info.mipLevels = m_description.mip_levels;
    image_create_info.arrayLayers = m_description.num_layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = nullptr;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // Usage
    if (m_description.sampled)
        image_create_info.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (m_description.storage)
        image_create_info.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (m_description.attachment && ImageUtils::is_depth_format(m_description.format))
        image_create_info.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if (m_description.attachment)
        image_create_info.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Flags
    if (m_description.cubemap)
        image_create_info.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

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

    // Create image view
    create_image_view();
}

VulkanImage::~VulkanImage() {
    vkDestroyImage(VulkanContext.device->handle(), m_image, nullptr);
    vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);

    vkDestroyImageView(VulkanContext.device->handle(), m_image_view, nullptr);
}

VkFormat VulkanImage::get_image_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
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
    case VK_IMAGE_TYPE_MAX_ENUM:
        assert(false && "Not supported");
        break;
    }
}

void VulkanImage::create_image_view() {
    // Create image view
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_image;
    view_create_info.viewType =
        m_description.cubemap ? VK_IMAGE_VIEW_TYPE_CUBE : get_image_view_type(m_description.type);
    view_create_info.format = get_image_format(m_description.format);

    if (ImageUtils::is_depth_format(m_description.format))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = m_description.mip_levels;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = m_description.num_layers;

    // TODO: Create mip image views

    VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &m_image_view));
}

} // namespace Mizu::Vulkan
