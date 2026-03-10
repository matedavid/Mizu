#include "vulkan_buffer_resource.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_device_memory_allocator.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc) : m_description(desc)
{
    QueueFamiliesArray queue_families;
    const VkBufferCreateInfo buffer_create_info = get_vulkan_buffer_create_info(m_description, queue_families);
    MIZU_ASSERT(
        buffer_create_info.usage != 0, "Failed to create buffer '{}', no usage was specified", m_description.name);

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &buffer_create_info, nullptr, &m_handle));

    if (!m_description.is_virtual)
    {
        m_allocation_info = VulkanContext.default_device_allocator->allocate_buffer_resource(*this);

        if (m_description.usage & BufferUsageBits::HostVisible)
        {
            map();
        }
    }

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanBufferResource::~VulkanBufferResource()
{
    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr)
            continue;

        const VulkanBufferResourceView* internal = get_internal_buffer_resource_view(view);
        delete internal;
    }

    unmap();
    if (!m_description.is_virtual)
        VulkanContext.default_device_allocator->release(m_allocation_info.id);

    vkDestroyBuffer(VulkanContext.device->handle(), m_handle, nullptr);
}

ResourceView VulkanBufferResource::as_srv(const BufferResourceViewDescription& desc)
{
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

ResourceView VulkanBufferResource::as_uav(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::UnorderedAccess,
        "Trying to create uav for buffer '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

ResourceView VulkanBufferResource::as_cbv(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::ConstantBuffer,
        "Trying to create cbv for buffer '{}' that was not created with ConstantBuffer usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ConstantBufferView, desc);
}

ResourceView VulkanBufferResource::get_or_create_resource_view(
    ResourceViewType type,
    const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        desc.is_valid(m_description.size),
        "Trying to create resource view with invalid description for buffer '{}'",
        m_description.name);

    for (const ResourceView& view : m_resource_views)
    {
        if (view.internal == nullptr || view.view_type != type)
            continue;

        const VulkanBufferResourceView* internal_view = get_internal_buffer_resource_view(view);
        if (internal_view->offset == desc.offset && internal_view->size == desc.size)
            return view;
    }

    // Create new view
    VulkanBufferResourceView* internal = new VulkanBufferResourceView{};
    internal->offset = desc.offset;
    internal->size = desc.size;
    internal->handle = m_handle;

    ResourceView view{};
    view.view_type = type;
    view.internal = internal;

    m_resource_views.push_back(view);

    return view;
}

MemoryRequirements VulkanBufferResource::get_memory_requirements() const
{
    return get_vulkan_buffer_memory_requirements(m_description);
}

uint8_t* VulkanBufferResource::map()
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::HostVisible,
        "Can't map buffer that does not have the HostVisible usage");

    if (m_mapped_data != nullptr)
        return m_mapped_data;

    VkDeviceMemory memory = static_cast<VkDeviceMemory>(m_allocation_info.device_memory);
    VK_CHECK(vkMapMemory(
        VulkanContext.device->handle(),
        memory,
        m_allocation_info.offset,
        m_allocation_info.size,
        0,
        reinterpret_cast<void**>(&m_mapped_data)));

    return m_mapped_data;
}

void VulkanBufferResource::unmap()
{
    if (m_mapped_data == nullptr)
        return;

    VkDeviceMemory memory = static_cast<VkDeviceMemory>(m_allocation_info.device_memory);
    vkUnmapMemory(VulkanContext.device->handle(), memory);
    m_mapped_data = nullptr;
}

VkBufferCreateInfo VulkanBufferResource::get_vulkan_buffer_create_info(
    const BufferDescription& desc,
    QueueFamiliesArray& queue_families)
{
    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
    {
        get_vulkan_queue_families_array(desc.queue_families, queue_families);
    }

    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = desc.size;
    buffer_create_info.usage = get_vulkan_buffer_usage(desc.usage);
    buffer_create_info.sharingMode = get_vulkan_sharing_mode(desc.sharing_mode);
    buffer_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
    buffer_create_info.pQueueFamilyIndices = queue_families.data();

    return buffer_create_info;
}

MemoryRequirements get_vulkan_buffer_memory_requirements(const BufferDescription& desc)
{
    QueueFamiliesArray queue_families;
    const VkBufferCreateInfo image_create_info =
        VulkanBufferResource::get_vulkan_buffer_create_info(desc, queue_families);

    VkDeviceBufferMemoryRequirements buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    buffer_info.pCreateInfo = &image_create_info;

    VkMemoryRequirements2 memory_reqs{};
    memory_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceBufferMemoryRequirements(VulkanContext.device->handle(), &buffer_info, &memory_reqs);

    MemoryRequirements reqs{};
    reqs.size = memory_reqs.memoryRequirements.size;
    reqs.alignment = memory_reqs.memoryRequirements.alignment;

    return reqs;
}

} // namespace Mizu::Vulkan