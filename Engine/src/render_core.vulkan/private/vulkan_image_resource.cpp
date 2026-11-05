#include "vulkan_image_resource.h"

#include <algorithm>
#include <ranges>
#include <vector>

#include "base/debug/assert.h"
#include "base/utils/hash.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanImageResource::VulkanImageResource(const ImageDescription& desc, const MemoryPlacement& placement)
    : m_sparse(false)
    , m_description(desc)
    , m_owns_resources(true)
{
    QueueFamiliesArray queue_families;
    const VkImageCreateInfo image_create_info = get_vulkan_image_create_info(m_description, queue_families);

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_handle));
    VK_DEBUG_SET_OBJECT_NAME(m_handle, VK_OBJECT_TYPE_IMAGE, m_description.name);

    init_memory_requirements();
    validate_placement(placement, m_memory_requirements, m_description.name);

    const VulkanGpuHeap& native_heap = static_cast<const VulkanGpuHeap&>(*placement.heap);
    VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), m_handle, native_heap.handle(), placement.offset));
}

VulkanImageResource::VulkanImageResource(const ImageDescription& desc, SparseTag)
    : m_sparse(true)
    , m_description(desc)
    , m_owns_resources(true)
{
    QueueFamiliesArray queue_families;
    VkImageCreateInfo image_create_info = get_vulkan_image_create_info(m_description, queue_families);
    image_create_info.flags |= VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_handle));
    VK_DEBUG_SET_OBJECT_NAME(m_handle, VK_OBJECT_TYPE_IMAGE, m_description.name);

    init_memory_requirements();
}

VulkanImageResource::VulkanImageResource(
    uint32_t width,
    uint32_t height,
    ImageFormat format,
    ImageUsageBits usage,
    ImageFlagBits flags,
    VkImage image,
    bool owns_resources)
    : m_sparse(false)
    , m_description({})
{
    m_description.width = width;
    m_description.height = height;
    m_description.format = format;
    m_description.usage = usage;
    m_description.flags = flags;

    m_owns_resources = owns_resources;

    m_handle = image;
}

VulkanImageResource::~VulkanImageResource()
{
    for (const VulkanImageResourceView& view : m_resource_views | std::views::values)
    {
        if (view.handle != VK_NULL_HANDLE)
        {
            free_image_view(view.handle);
        }
    }

    if (m_owns_resources)
    {
        vkDestroyImage(VulkanContext.device->handle(), m_handle, nullptr);
    }
}

VulkanImageResourceView VulkanImageResource::as_srv(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Sampled,
        "Trying to create srv for image '{}' that was not created with Sampled usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

VulkanImageResourceView VulkanImageResource::as_uav(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::UnorderedAccess,
        "Trying to create uav for image '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

VulkanImageResourceView VulkanImageResource::as_rtv(const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & ImageUsageBits::Attachment,
        "Trying to create rtv for image '{}' that was not created with Attachment usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::RenderTargetView, desc);
}

VulkanImageResourceView VulkanImageResource::get_or_create_resource_view(
    ResourceViewType type,
    const ImageResourceViewDescription& desc)
{
    MIZU_ASSERT(
        desc.is_valid(m_description.num_mips, m_description.num_layers),
        "Trying to create resource view with invalid description for image '{}'",
        m_description.name);

    const size_t hash = hash_compute(desc.hash(), type);

    const auto it = m_resource_views.find(hash);
    if (it != m_resource_views.end())
        return it->second;

    VulkanImageResourceView resource_view{};
    resource_view.description = desc;
    resource_view.format = desc.override_format.value_or(m_description.format);
    resource_view.type = type;
    resource_view.handle = create_image_view(*this, desc);

    m_resource_views.emplace(hash, resource_view);

    return resource_view;
}

MemoryRequirements VulkanImageResource::get_memory_requirements() const
{
    return m_memory_requirements;
}

ImageMemoryRequirements VulkanImageResource::get_image_memory_requirements() const
{
    return m_image_memory_requirements;
}

SparseImageMemoryRequirements VulkanImageResource::get_sparse_memory_requirements() const
{
    MIZU_ASSERT(m_sparse, "Trying to get sparse memory requirements of non sparse image '{}'", m_description.name);
    return m_sparse_requirements;
}

uint64_t VulkanImageResource::get_mip_tail_resource_offset(uint32_t array_layer) const
{
    MIZU_ASSERT(m_sparse, "Trying to get mip tail offset of non sparse image '{}'", m_description.name);

    if (m_sparse_requirements.single_mip_tail)
        return m_mip_tail_offset;

    return m_mip_tail_offset + static_cast<uint64_t>(array_layer) * m_mip_tail_stride;
}

void VulkanImageResource::init_memory_requirements()
{
    VkImageMemoryRequirementsInfo2 info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2;
    info.image = m_handle;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 memory_reqs2{};
    memory_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memory_reqs2.pNext = &dedicated_reqs;
    vkGetImageMemoryRequirements2(VulkanContext.device->handle(), &info, &memory_reqs2);

    const VkMemoryRequirements& memory_reqs = memory_reqs2.memoryRequirements;
    m_memory_requirements = get_vulkan_memory_requirements(memory_reqs, dedicated_reqs);

    m_image_memory_requirements = ImageMemoryRequirements{
        .size = memory_reqs.size,
        .offset = 0, // Images are always allocated at offset 0 within their allocation
        .row_pitch = m_description.width * get_image_format_size(m_description.format),
    };

    if (!m_sparse)
        return;

    uint32_t sparse_reqs_count = 0;
    vkGetImageSparseMemoryRequirements(VulkanContext.device->handle(), m_handle, &sparse_reqs_count, nullptr);

    std::vector<VkSparseImageMemoryRequirements> sparse_reqs(sparse_reqs_count);
    vkGetImageSparseMemoryRequirements(
        VulkanContext.device->handle(), m_handle, &sparse_reqs_count, sparse_reqs.data());

    const VkImageAspectFlags aspect =
        is_depth_format(m_description.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    const auto reqs_it = std::ranges::find_if(sparse_reqs, [aspect](const VkSparseImageMemoryRequirements& reqs) {
        return (reqs.formatProperties.aspectMask & aspect) != 0;
    });

    if (reqs_it == sparse_reqs.end())
    {
        MIZU_ASSERT(
            false,
            "Failed to get sparse memory requirements for image '{}', is the format supported for sparse residency?",
            m_description.name);
        return;
    }

    const VkSparseImageMemoryRequirements& reqs = *reqs_it;
    const VkExtent3D& granularity = reqs.formatProperties.imageGranularity;

    m_sparse_requirements.memory = m_memory_requirements;
    m_sparse_requirements.page_size = memory_reqs.alignment;
    m_sparse_requirements.tile_extent = glm::uvec3(granularity.width, granularity.height, granularity.depth);
    m_sparse_requirements.mip_tail_first_mip = std::min(reqs.imageMipTailFirstLod, m_description.num_mips);
    m_sparse_requirements.mip_tail_size = reqs.imageMipTailSize;
    m_sparse_requirements.single_mip_tail =
        (reqs.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0;

    m_mip_tail_offset = reqs.imageMipTailOffset;
    m_mip_tail_stride = reqs.imageMipTailStride;

    const auto metadata_it = std::ranges::find_if(sparse_reqs, [](const VkSparseImageMemoryRequirements& reqs) {
        return (reqs.formatProperties.aspectMask & VK_IMAGE_ASPECT_METADATA_BIT) != 0;
    });

    if (metadata_it == sparse_reqs.end())
        return;

    const VkSparseImageMemoryRequirements& metadata_reqs = *metadata_it;
    const bool single_metadata =
        (metadata_reqs.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT) != 0;

    m_metadata_offset = metadata_reqs.imageMipTailOffset;
    m_metadata_stride = metadata_reqs.imageMipTailStride;
    m_metadata_region_size = metadata_reqs.imageMipTailSize;
    m_metadata_region_count = single_metadata ? 1 : m_description.num_layers;

    MIZU_ASSERT(
        m_metadata_region_size % memory_reqs.alignment == 0,
        "Sparse metadata size of image '{}' is not a multiple of the page size",
        m_description.name);

    m_sparse_requirements.metadata_size = m_metadata_region_size * m_metadata_region_count;
}

uint64_t VulkanImageResource::get_metadata_resource_offset(uint32_t region) const
{
    MIZU_ASSERT(region < m_metadata_region_count, "Invalid metadata region {}", region);
    return m_metadata_offset + static_cast<uint64_t>(region) * m_metadata_stride;
}

VkImageCreateInfo get_vulkan_image_create_info(const ImageDescription& desc, QueueFamiliesArray& queue_families)
{
    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
    {
        get_vulkan_queue_families_array(desc.queue_families, queue_families);
    }

    // If we only have one queue family requested, force sharing mode to exclusive
    const ResourceSharingMode sharing_mode =
        queue_families.size() > 1 ? desc.sharing_mode : ResourceSharingMode::Exclusive;

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.flags = get_vulkan_image_flags(desc.flags, desc.type);
    image_create_info.imageType = get_vulkan_image_type(desc.type);
    image_create_info.format = get_vulkan_image_format(desc.format);
    image_create_info.extent = VkExtent3D{.width = desc.width, .height = desc.height, .depth = desc.depth};
    image_create_info.mipLevels = desc.num_mips;
    image_create_info.arrayLayers = desc.num_layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.usage = get_vulkan_image_usage(desc.usage, desc.format);
    image_create_info.sharingMode = get_vulkan_sharing_mode(sharing_mode);
    image_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
    image_create_info.pQueueFamilyIndices = queue_families.data();
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return image_create_info;
}

MemoryRequirements get_vulkan_image_memory_requirements(const ImageDescription& desc)
{
    QueueFamiliesArray queue_families;
    const VkImageCreateInfo image_create_info = get_vulkan_image_create_info(desc, queue_families);

    VkDeviceImageMemoryRequirements image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
    image_info.pCreateInfo = &image_create_info;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 memory_reqs{};
    memory_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memory_reqs.pNext = &dedicated_reqs;
    vkGetDeviceImageMemoryRequirements(VulkanContext.device->handle(), &image_info, &memory_reqs);

    return get_vulkan_memory_requirements(memory_reqs.memoryRequirements, dedicated_reqs);
}

} // namespace Mizu::Vulkan