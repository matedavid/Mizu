#include "vulkan_gpu_heap.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_debug.h"

namespace Mizu::Vulkan
{

VulkanGpuHeap::VulkanGpuHeap(GpuHeapDescription desc) : m_description(std::move(desc))
{
    const VkPhysicalDeviceMemoryProperties& memory_properties = VulkanContext.device->get_memory_properties();
    MIZU_ASSERT(
        m_description.memory_type < memory_properties.memoryTypeCount,
        "Invalid memory type {} for heap '{}'",
        m_description.memory_type,
        m_description.name);

    const VkMemoryPropertyFlags property_flags = memory_properties.memoryTypes[m_description.memory_type].propertyFlags;
    const VkMemoryPropertyFlags required_flags =
        is_host_visible() ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    MIZU_ASSERT(
        (property_flags & required_flags) == required_flags,
        "Memory type {} of heap '{}' does not match its domain {}",
        m_description.memory_type,
        m_description.name,
        meta::enum_name(m_description.domain));

    VkMemoryAllocateFlagsInfo allocate_flags_info{};
    allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocate_flags_info.pNext = nullptr;
    allocate_flags_info.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = VulkanContext.device->is_buffer_device_address_enabled() ? &allocate_flags_info : nullptr;
    allocate_info.allocationSize = m_description.size;
    allocate_info.memoryTypeIndex = m_description.memory_type;
    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));

    VK_DEBUG_SET_OBJECT_NAME(m_memory, VK_OBJECT_TYPE_DEVICE_MEMORY, m_description.name);

    if (is_host_visible())
    {
        VK_CHECK(vkMapMemory(
            VulkanContext.device->handle(), m_memory, 0, VK_WHOLE_SIZE, 0, reinterpret_cast<void**>(&m_mapped_memory)));
        MIZU_ASSERT(m_mapped_memory != nullptr, "Failed to map heap '{}'", m_description.name);
    }
}

VulkanGpuHeap::~VulkanGpuHeap()
{
    vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
}

bool VulkanGpuHeap::is_compatible(const MemoryRequirements& reqs) const
{
    return (reqs.memory_type_bits & (1u << m_description.memory_type)) != 0;
}

uint8_t* VulkanGpuHeap::get_mapped_memory() const
{
    MIZU_ASSERT(is_host_visible(), "Trying to get mapped data for non-host-visible memory heap");
    return m_mapped_memory;
}

} // namespace Mizu::Vulkan
