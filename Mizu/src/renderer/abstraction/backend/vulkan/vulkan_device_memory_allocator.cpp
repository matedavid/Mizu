#include "vulkan_device_memory_allocator.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

//
// VulkanBaseDeviceMemoryAllocator
//

VulkanBaseDeviceMemoryAllocator::~VulkanBaseDeviceMemoryAllocator() {
#if MIZU_DEBUG
    if (!m_memory_allocations.empty()) {
        MIZU_LOG_ERROR("Some memory chunks were not released manually ({}), this could cause problems",
                       m_memory_allocations.size());
    }
#endif

    for (const auto& [_, memory] : m_memory_allocations) {
        vkFreeMemory(VulkanContext.device->handle(), memory, nullptr);
    }

    m_memory_allocations.clear();
}

Allocation VulkanBaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image) {
    const auto& native_image = dynamic_cast<const VulkanImageResource&>(image);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), native_image.get_image_handle(), &memory_requirements);

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MIZU_ASSERT(memory_type_index.has_value(), "No suitable memory to allocate image");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VkDeviceMemory memory;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &memory));
    VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), native_image.get_image_handle(), memory, 0));

    const auto id = Allocation();
    m_memory_allocations.insert({id, memory});

    return id;
}

void VulkanBaseDeviceMemoryAllocator::release(Allocation id) {
    const auto it = m_memory_allocations.find(id);
    if (it == m_memory_allocations.end()) {
        MIZU_LOG_WARNING("Allocation {} does not exist", static_cast<UUID::Type>(id));
        return;
    }

    vkFreeMemory(VulkanContext.device->handle(), it->second, nullptr);
    m_memory_allocations.erase(it);
}

} // namespace Mizu::Vulkan
