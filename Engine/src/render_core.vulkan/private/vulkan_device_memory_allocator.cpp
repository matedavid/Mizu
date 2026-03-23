#include "vulkan_device_memory_allocator.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_image_resource.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

//
// VulkanBaseDeviceMemoryAllocator
//

VulkanBaseDeviceMemoryAllocator::~VulkanBaseDeviceMemoryAllocator()
{
#if MIZU_DEBUG
    if (!m_memory_allocations.empty())
    {
        MIZU_LOG_ERROR(
            "Some memory chunks were not released manually ({}), this could cause problems",
            m_memory_allocations.size());
    }
#endif

    reset();
}

AllocationInfo VulkanBaseDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer)
{
    const VulkanBufferResource& native_buffer = dynamic_cast<const VulkanBufferResource&>(buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), native_buffer.handle(), &memory_requirements);

    VkMemoryPropertyFlags memory_property_flags = 0;
    VkMemoryAllocateFlags memory_allocate_flags = 0;
    {
        if (native_buffer.get_usage() & BufferUsageBits::HostVisible)
        {
            memory_property_flags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
        else
        {
            memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        const VkBufferUsageFlags vk_usage_flags = get_vulkan_buffer_usage(native_buffer.get_usage());
        if (vk_usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            memory_allocate_flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        }
    }

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, memory_property_flags);
    MIZU_ASSERT(memory_type_index.has_value(), "No suitable memory to allocate image");

    VkMemoryAllocateFlagsInfo allocate_flags_info{};
    allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocate_flags_info.flags = memory_allocate_flags;

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = &allocate_flags_info;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VkDeviceMemory memory;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), native_buffer.handle(), memory, 0));

    AllocationInfo info{};
    info.id = AllocationId();
    info.size = memory_requirements.size;
    info.offset = 0;
    info.device_memory = (void*)memory;

    m_memory_allocations.insert({info.id, memory});

    return info;
}

AllocationInfo VulkanBaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image)
{
    const VulkanImageResource& native_image = dynamic_cast<const VulkanImageResource&>(image);

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), native_image.handle(), &memory_requirements);

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MIZU_ASSERT(memory_type_index.has_value(), "No suitable memory to allocate image");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VkDeviceMemory memory;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &memory));
    VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), native_image.handle(), memory, 0));

    AllocationInfo info{};
    info.id = AllocationId();
    info.size = memory_requirements.size;
    info.offset = 0;
    info.device_memory = (void*)memory;

    m_memory_allocations.insert({info.id, memory});

    return info;
}

void VulkanBaseDeviceMemoryAllocator::release(AllocationId id)
{
    const auto it = m_memory_allocations.find(id);
    if (it == m_memory_allocations.end())
    {
        MIZU_LOG_WARNING("Allocation {} does not exist", static_cast<UUID::Type>(id));
        return;
    }

    vkFreeMemory(VulkanContext.device->handle(), it->second, nullptr);
    m_memory_allocations.erase(it);
}

void VulkanBaseDeviceMemoryAllocator::reset()
{
    for (const auto& [_, memory] : m_memory_allocations)
    {
        vkFreeMemory(VulkanContext.device->handle(), memory, nullptr);
    }

    m_memory_allocations.clear();
}

//
// VulkanTransientMemoryPool
//

VulkanTransientMemoryPool::VulkanTransientMemoryPool(std::string_view name) : m_name(name) {}

VulkanTransientMemoryPool::~VulkanTransientMemoryPool()
{
    reset();
}

void VulkanTransientMemoryPool::place_buffer(BufferResource& buffer, size_t offset)
{
    VulkanBufferResource& native_buffer = static_cast<VulkanBufferResource&>(buffer);

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), native_buffer.handle(), &reqs);

    m_buffer_infos.emplace_back(native_buffer, reqs.size, offset, reqs.memoryTypeBits);
}

void VulkanTransientMemoryPool::place_image(ImageResource& image, size_t offset)
{
    VulkanImageResource& native_image = static_cast<VulkanImageResource&>(image);

    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), native_image.handle(), &reqs);

    m_image_infos.emplace_back(native_image, reqs.size, offset, reqs.memoryTypeBits);
}

void VulkanTransientMemoryPool::commit()
{
    MIZU_PROFILE_SCOPED;

    if (m_buffer_infos.empty() && m_image_infos.empty())
    {
        return;
    }

    VkMemoryAllocateFlags memory_allocate_flags = 0;
    uint32_t memory_type_bits = std::numeric_limits<uint32_t>::max();
    uint64_t max_size = 0;

    for (const BufferInfo& info : m_buffer_infos)
    {
        memory_type_bits &= info.memory_type_bits;
        max_size = std::max(info.offset + info.size, max_size);

        const VkBufferUsageFlags usage_flags = get_vulkan_buffer_usage(info.buffer.get_usage());
        if (usage_flags & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            memory_allocate_flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        }
    }

    for (const ImageInfo& info : m_image_infos)
    {
        memory_type_bits &= info.memory_type_bits;
        max_size = std::max(info.offset + info.size, max_size);
    }

    const auto memory_index =
        VulkanContext.device->find_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MIZU_ASSERT(
        memory_index.has_value(), "Failed to find suitable memory type when committing VulkanTransientMemoryPool");

    if (m_memory == VK_NULL_HANDLE || max_size > m_size || *memory_index != m_memory_type_index)
    {
        m_size = max_size;
        m_memory_type_index = *memory_index;

        free_if_allocated();
        allocate_memory(m_size, memory_allocate_flags, m_memory_type_index);
    }

    bind_resources();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

void VulkanTransientMemoryPool::reset()
{
    VulkanContext.device->wait_idle();

    free_if_allocated();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

size_t VulkanTransientMemoryPool::get_committed_size() const
{
    return m_size;
}

void VulkanTransientMemoryPool::bind_resources()
{
    for (const BufferInfo& info : m_buffer_infos)
    {
        if (info.buffer.get_allocation_info().device_memory != nullptr)
            continue;

        VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), info.buffer.handle(), m_memory, info.offset));

        AllocationInfo allocation_info{};
        allocation_info.size = info.size;
        allocation_info.offset = info.offset;
        allocation_info.device_memory = static_cast<void*>(m_memory);
        info.buffer.set_allocation_info(allocation_info);
    }

    for (const ImageInfo& info : m_image_infos)
    {
        if (info.image.get_allocation_info().device_memory != nullptr)
            continue;

        VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), info.image.handle(), m_memory, info.offset));

        AllocationInfo allocation_info{};
        allocation_info.size = info.size;
        allocation_info.offset = info.offset;
        allocation_info.device_memory = static_cast<void*>(m_memory);
        info.image.set_allocation_info(allocation_info);
    }
}

void VulkanTransientMemoryPool::allocate_memory(
    size_t size,
    VkMemoryAllocateFlags allocate_flags,
    uint32_t memory_type_index)
{
    MIZU_PROFILE_SCOPED;

    VkMemoryAllocateFlagsInfo allocate_flags_info{};
    allocate_flags_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocate_flags_info.flags = allocate_flags;

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.pNext = &allocate_flags_info;
    allocate_info.allocationSize = size;
    allocate_info.memoryTypeIndex = memory_type_index;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));
    VK_DEBUG_SET_OBJECT_NAME(m_memory, m_name);
}

void VulkanTransientMemoryPool::free_if_allocated()
{
    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

} // namespace Mizu::Vulkan
