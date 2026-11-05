#include "vulkan_buffer_resource.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_resource_view.h"

namespace Mizu::Vulkan
{

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc, const MemoryPlacement& placement)
    : m_sparse(false)
    , m_description(desc)
{
    QueueFamiliesArray queue_families;
    const VkBufferCreateInfo buffer_create_info = get_vulkan_buffer_create_info(m_description, queue_families);
    MIZU_ASSERT(
        buffer_create_info.usage != 0, "Failed to create buffer '{}', no usage was specified", m_description.name);

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &buffer_create_info, nullptr, &m_handle));
    VK_DEBUG_SET_OBJECT_NAME(m_handle, VK_OBJECT_TYPE_BUFFER, m_description.name);

    init_memory_requirements();
    validate_placement(placement, m_memory_requirements, m_description.name);

    const VulkanGpuHeap& native_heap = static_cast<const VulkanGpuHeap&>(*placement.heap);
    VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), m_handle, native_heap.handle(), placement.offset));

    if (native_heap.is_host_visible())
    {
        m_mapped_memory = native_heap.get_mapped_memory() + placement.offset;
    }
}

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc, SparseTag)
    : m_sparse(true)
    , m_description(desc)
{
    QueueFamiliesArray queue_families;
    VkBufferCreateInfo buffer_create_info = get_vulkan_buffer_create_info(m_description, queue_families);
    MIZU_ASSERT(
        buffer_create_info.usage != 0, "Failed to create buffer '{}', no usage was specified", m_description.name);

    buffer_create_info.flags |= VK_BUFFER_CREATE_SPARSE_BINDING_BIT | VK_BUFFER_CREATE_SPARSE_RESIDENCY_BIT;

    m_sparse = true;

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &buffer_create_info, nullptr, &m_handle));
    VK_DEBUG_SET_OBJECT_NAME(m_handle, VK_OBJECT_TYPE_BUFFER, m_description.name);

    init_memory_requirements();
}

VulkanBufferResource::~VulkanBufferResource()
{
    vkDestroyBuffer(VulkanContext.device->handle(), m_handle, nullptr);
}

VulkanBufferResourceView VulkanBufferResource::as_srv(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::ShaderResource,
        "Trying to create srv for buffer '{}' that was not created with ShaderResource usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ShaderResourceView, desc);
}

VulkanBufferResourceView VulkanBufferResource::as_uav(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::UnorderedAccess,
        "Trying to create uav for buffer '{}' that was not created with UnorderedAccess usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::UnorderedAccessView, desc);
}

VulkanBufferResourceView VulkanBufferResource::as_cbv(const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::ConstantBuffer,
        "Trying to create cbv for buffer '{}' that was not created with ConstantBuffer usage",
        m_description.name);
    return get_or_create_resource_view(ResourceViewType::ConstantBufferView, desc);
}

VulkanBufferResourceView VulkanBufferResource::get_or_create_resource_view(
    ResourceViewType type,
    const BufferResourceViewDescription& desc)
{
    MIZU_ASSERT(
        desc.is_valid(m_description.size),
        "Trying to create resource view with invalid description for buffer '{}'",
        m_description.name);

    VulkanBufferResourceView resource_view{};
    resource_view.offset = desc.offset;
    resource_view.size = desc.size;
    resource_view.stride = desc.stride;
    resource_view.type = type;
    resource_view.handle = m_handle;

    return resource_view;
}

MemoryRequirements VulkanBufferResource::get_memory_requirements() const
{
    return m_memory_requirements;
}

void VulkanBufferResource::init_memory_requirements()
{
    VkBufferMemoryRequirementsInfo2 info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    info.buffer = m_handle;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 memory_reqs{};
    memory_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memory_reqs.pNext = &dedicated_reqs;
    vkGetBufferMemoryRequirements2(VulkanContext.device->handle(), &info, &memory_reqs);

    m_memory_requirements = get_vulkan_memory_requirements(memory_reqs.memoryRequirements, dedicated_reqs);
}

uint8_t* VulkanBufferResource::get_mapped_memory() const
{
    return m_mapped_memory;
}

VkBufferCreateInfo get_vulkan_buffer_create_info(const BufferDescription& desc, QueueFamiliesArray& queue_families)
{
    if (desc.sharing_mode == ResourceSharingMode::Concurrent)
    {
        get_vulkan_queue_families_array(desc.queue_families, queue_families);
    }

    // If we only have one queue family requested, force sharing mode to exclusive
    const ResourceSharingMode sharing_mode =
        queue_families.size() > 1 ? desc.sharing_mode : ResourceSharingMode::Exclusive;

    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.flags = 0;
    buffer_create_info.size = desc.size;
    buffer_create_info.usage = get_vulkan_buffer_usage(desc.usage);
    buffer_create_info.sharingMode = get_vulkan_sharing_mode(sharing_mode);
    buffer_create_info.queueFamilyIndexCount = static_cast<uint32_t>(queue_families.size());
    buffer_create_info.pQueueFamilyIndices = queue_families.data();

    return buffer_create_info;
}

MemoryRequirements get_vulkan_buffer_memory_requirements(const BufferDescription& desc)
{
    QueueFamiliesArray queue_families;
    const VkBufferCreateInfo image_create_info = get_vulkan_buffer_create_info(desc, queue_families);

    VkDeviceBufferMemoryRequirements buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    buffer_info.pCreateInfo = &image_create_info;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 memory_reqs{};
    memory_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    memory_reqs.pNext = &dedicated_reqs;
    vkGetDeviceBufferMemoryRequirements(VulkanContext.device->handle(), &buffer_info, &memory_reqs);

    return get_vulkan_memory_requirements(memory_reqs.memoryRequirements, dedicated_reqs);
}

} // namespace Mizu::Vulkan
