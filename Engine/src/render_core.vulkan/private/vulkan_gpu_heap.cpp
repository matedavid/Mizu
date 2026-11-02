#include "vulkan_gpu_heap.h"

#include "base/debug/assert.h"

#include "vulkan_context.h"
#include "vulkan_debug.h"

namespace Mizu::Vulkan
{

VulkanGpuHeap::VulkanGpuHeap(GpuHeapDescription desc) : m_description(std::move(desc))
{
    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(m_description.domain, m_description.memory_type_bits);
    MIZU_ASSERT(
        memory_type_index.has_value(),
        "Failed to find_memory_type for domain={}, memory_type_bits={}",
        meta::enum_name(m_description.domain),
        m_description.memory_type_bits);

    m_memory_type_index = *memory_type_index;

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = nullptr;
    allocate_info.allocationSize = m_description.size;
    allocate_info.memoryTypeIndex = m_memory_type_index;
    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));

    VK_DEBUG_SET_OBJECT_NAME(m_memory, VK_OBJECT_TYPE_DEVICE_MEMORY, m_description.name);

    if (is_host_visible())
    {
        VkMemoryMapInfo map_info{};
        map_info.sType = VK_STRUCTURE_TYPE_MEMORY_MAP_INFO;
        map_info.pNext = nullptr;
        map_info.flags = 0;
        map_info.memory = m_memory;
        map_info.offset = 0;
        map_info.size = m_description.size;

        vkMapMemory2(VulkanContext.device->handle(), &map_info, reinterpret_cast<void**>(&m_mapped_memory));
    }
}

VulkanGpuHeap::~VulkanGpuHeap()
{
    vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
}

bool VulkanGpuHeap::is_compatible(const MemoryRequirements& reqs) const
{
    return (reqs.memory_type_bits & (1u << m_memory_type_index)) != 0;
}

uint8_t* VulkanGpuHeap::get_mapped_memory() const
{
    MIZU_ASSERT(is_host_visible(), "Trying to get mapped data for non-host-visible memory heap");
    return m_mapped_memory;
}

} // namespace Mizu::Vulkan
