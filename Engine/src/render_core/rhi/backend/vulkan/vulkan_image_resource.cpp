#include "vulkan_image_resource.h"

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu::Vulkan
{

VulkanImageResource::VulkanImageResource(const ImageDescription& desc) : m_description(desc)
{
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = get_vulkan_image_type(m_description.type);
    image_create_info.format = get_vulkan_image_format(m_description.format);
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
    image_create_info.flags = m_description.type == ImageType::Cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    image_create_info.usage = get_vulkan_usage(m_description.usage, m_description.format);

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_handle));

    if (!m_description.is_virtual)
    {
        m_allocation_info = Renderer::get_allocator()->allocate_image_resource(*this);
    }

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanImageResource::VulkanImageResource(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    ImageUsageBits usage,
    VkImage image,
    bool owns_resources)
    : m_description({})
{
    m_description.width = width;
    m_description.height = height;
    m_description.format = format;
    m_description.usage = usage;

    m_owns_resources = owns_resources;

    m_handle = image;
}

VulkanImageResource::~VulkanImageResource()
{
    if (!m_description.is_virtual && m_owns_resources)
    {
        Renderer::get_allocator()->release(m_allocation_info.id);
    }

    if (m_owns_resources)
    {
        vkDestroyImage(VulkanContext.device->handle(), m_handle, nullptr);
    }
}

MemoryRequirements VulkanImageResource::get_memory_requirements() const
{
    VkImageMemoryRequirementsInfo2 vk_reqs_info2{};
    vk_reqs_info2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    vk_reqs_info2.image = m_handle;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 vk_reqs2{};
    vk_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vk_reqs2.pNext = &dedicated_reqs;

    vkGetImageMemoryRequirements2(VulkanContext.device->handle(), &vk_reqs_info2, &vk_reqs2);

    MemoryRequirements reqs{};
    reqs.size = vk_reqs2.memoryRequirements.size;
    reqs.alignment = vk_reqs2.memoryRequirements.alignment;

    return reqs;
}

ImageMemoryRequirements VulkanImageResource::get_image_memory_requirements() const
{
    ImageMemoryRequirements reqs{};
    reqs.size = m_allocation_info.size;
    reqs.offset = 0; // Images are always allocated at offset 0 within their allocation
    reqs.row_pitch = m_description.width * ImageUtils::get_format_size(m_description.format);

    return reqs;
}

VkImageType VulkanImageResource::get_vulkan_image_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return VK_IMAGE_TYPE_1D;
    case ImageType::Image2D:
        return VK_IMAGE_TYPE_2D;
    case ImageType::Image3D:
        return VK_IMAGE_TYPE_3D;
    case ImageType::Cubemap:
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkImageCreateInfo-flags-00949
        return VK_IMAGE_TYPE_2D;
    }
}

VkFormat VulkanImageResource::get_vulkan_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case ImageFormat::RG16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormat::RG32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormat::RGB32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case ImageFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::RGBA32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::BGRA8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case ImageFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    }
}

VkImageLayout VulkanImageResource::get_vulkan_image_resource_state(ImageResourceState state)
{
    switch (state)
    {
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
    case ImageResourceState::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}

VkImageUsageFlags VulkanImageResource::get_vulkan_usage(ImageUsageBits usage, ImageFormat format)
{
    VkImageUsageFlags vulkan_usage = 0;

    const bool has_usage_attachment = usage & ImageUsageBits::Attachment;
    if (has_usage_attachment && ImageUtils::is_depth_format(format))
        vulkan_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if (has_usage_attachment)
        vulkan_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & ImageUsageBits::Sampled)
        vulkan_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & ImageUsageBits::Storage)
        vulkan_usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (usage & ImageUsageBits::TransferDst)
        vulkan_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return vulkan_usage;
}

VkImageFormatProperties VulkanImageResource::get_format_properties() const
{
    const VkImageUsageFlags usage = get_vulkan_usage(m_description.usage, m_description.format);
    const uint32_t flags = m_description.type == ImageType::Cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VkImageFormatProperties properties;
    vkGetPhysicalDeviceImageFormatProperties(
        VulkanContext.device->physical_device(),
        get_vulkan_image_format(m_description.format),
        get_vulkan_image_type(m_description.type),
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        flags,
        &properties);

    return properties;
}

} // namespace Mizu::Vulkan