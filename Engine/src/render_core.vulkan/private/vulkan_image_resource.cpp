#include "vulkan_image_resource.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanImageResource::VulkanImageResource(const ImageDescription& desc) : m_description(desc)
{
    QueueFamiliesArray queue_families;
    const VkImageCreateInfo image_create_info = get_vulkan_image_create_info(m_description, queue_families);
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

ResourceView VulkanImageResource::as_srv(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Sampled,
        "Trying to create srv for image '{}' that was not created with Sampled usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

ResourceView VulkanImageResource::as_uav(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::UnorderedAccess,
        "Trying to create uav for image '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

ResourceView VulkanImageResource::as_rtv(const ImageResourceViewDescription& desc)
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
    MIZU_ASSERT(
        desc.is_valid(m_description.num_mips, m_description.num_layers),
        "Trying to create resource view with invalid description for image '{}'",
        m_description.name);

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
    return get_vulkan_image_memory_requirements(m_description);
}

ImageMemoryRequirements VulkanImageResource::get_image_memory_requirements() const
{
    ImageMemoryRequirements reqs{};
    reqs.size = m_allocation_info.size;
    reqs.offset = 0; // Images are always allocated at offset 0 within their allocation
    reqs.row_pitch = m_description.width * get_image_format_size(m_description.format);

    return reqs;
}

VkImageCreateInfo VulkanImageResource::get_vulkan_image_create_info(
    const ImageDescription& desc,
    QueueFamiliesArray& queue_families)
{
    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
    {
        get_vulkan_queue_families_array(desc.queue_families, queue_families);
    }

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.flags = desc.type == ImageType::Cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    image_create_info.imageType = get_vulkan_image_type(desc.type);
    image_create_info.format = get_vulkan_image_format(desc.format);
    image_create_info.extent = VkExtent3D{.width = desc.width, .height = desc.height, .depth = desc.depth};
    image_create_info.mipLevels = desc.num_mips;
    image_create_info.arrayLayers = desc.num_layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = get_vulkan_image_usage(desc.usage, desc.format);
    image_create_info.sharingMode = get_vulkan_sharing_mode(desc.sharing_mode);
    image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
    image_create_info.pQueueFamilyIndices = queue_families.data();
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return image_create_info;
}

MemoryRequirements get_vulkan_image_memory_requirements(const ImageDescription& desc)
{
    QueueFamiliesArray queue_families;
    const VkImageCreateInfo image_create_info = VulkanImageResource::get_vulkan_image_create_info(desc, queue_families);

    VkDeviceImageMemoryRequirements image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
    image_info.pCreateInfo = &image_create_info;

    VkMemoryRequirements2 memory_reqs{};
    memory_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceImageMemoryRequirements(VulkanContext.device->handle(), &image_info, &memory_reqs);

    MemoryRequirements reqs{};
    reqs.size = memory_reqs.memoryRequirements.size;
    reqs.alignment = memory_reqs.memoryRequirements.alignment;

    return reqs;
}

} // namespace Mizu::Vulkan