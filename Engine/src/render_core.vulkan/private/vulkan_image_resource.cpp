#include "vulkan_image_resource.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_resource_view.h"

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
        m_allocation_info = VulkanContext.default_device_allocator->allocate_image_resource(*this);
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
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr)
            continue;

        const VulkanImageResourceView* internal_view = get_internal_image_resource_view(view);
        free_image_view(internal_view->handle);
        delete internal_view;
    }

    if (!m_description.is_virtual && m_owns_resources)
    {
        VulkanContext.default_device_allocator->release(m_allocation_info.id);
    }

    if (m_owns_resources)
    {
        vkDestroyImage(VulkanContext.device->handle(), m_handle, nullptr);
    }
}

ResourceView VulkanImageResource::as_srv(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Sampled,
        "Trying to create srv for image '{}' that was not created with Sampled usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

ResourceView VulkanImageResource::as_uav(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::UnorderedAccess,
        "Trying to create uav for image '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

ResourceView VulkanImageResource::as_rtv(ImageResourceViewDescription desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Attachment,
        "Trying to create rtv for image '{}' that was not created with Attachment usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::RenderTargetView, desc);
}

ResourceView VulkanImageResource::get_or_create_resource_view(
    ResourceViewType type,
    const ImageResourceViewDescription& desc)
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr || view.view_type != type)
            continue;

        const VulkanImageResourceView* internal_view = get_internal_image_resource_view(view);
        if (internal_view->description == desc)
            return view;
    }

    // Create new view
    VulkanImageResourceView* internal = new VulkanImageResourceView{};
    internal->description = desc;
    // TODO: Don't like the override format logic here, it also exists in create_image_view, consider unifying
    internal->format = desc.override_format.value_or(m_description.format);
    internal->handle = create_image_view(desc, *this);

    ResourceView view{};
    view.view_type = type;
    view.internal = internal;

    m_resource_views.push_back(view);

    return view;
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
    reqs.row_pitch = m_description.width * get_image_format_size(m_description.format);

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
    case ImageFormat::R16G16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormat::R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormat::R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case ImageFormat::R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::R32G32B32A32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::B8G8R8A8_UNORM:
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
    case ImageResourceState::UnorderedAccess:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ImageResourceState::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
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
    if (has_usage_attachment && is_depth_format(format))
        vulkan_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if (has_usage_attachment)
        vulkan_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & ImageUsageBits::Sampled)
        vulkan_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & ImageUsageBits::UnorderedAccess)
        vulkan_usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (usage & ImageUsageBits::TransferSrc)
        vulkan_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

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
        VulkanContext.device->get_physical_device(),
        get_vulkan_image_format(m_description.format),
        get_vulkan_image_type(m_description.type),
        VK_IMAGE_TILING_OPTIMAL,
        usage,
        flags,
        &properties);

    return properties;
}

} // namespace Mizu::Vulkan