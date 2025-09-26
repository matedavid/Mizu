#include "vulkan_device_memory_allocator.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

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

        const VkBufferUsageFlags vk_usage_flags = VulkanBufferResource::get_vulkan_usage(native_buffer.get_usage());
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

    if (memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        void* mapped_data;
        VK_CHECK(vkMapMemory(VulkanContext.device->handle(), memory, info.offset, info.size, 0, &mapped_data));

        m_mapped_allocations.insert({info.id, mapped_data});
    }

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

uint8_t* VulkanBaseDeviceMemoryAllocator::get_mapped_memory(AllocationId id) const
{
    const auto it = m_mapped_allocations.find(id);
    MIZU_ASSERT(
        it != m_mapped_allocations.end(),
        "Could not find allocation with id {} that is mapped",
        static_cast<UUID::Type>(id));

    return reinterpret_cast<uint8_t*>(it->second);
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

    const auto mapped_it = m_mapped_allocations.find(id);
    if (mapped_it != m_mapped_allocations.end())
    {
        m_mapped_allocations.erase(mapped_it);
    }
}

void VulkanBaseDeviceMemoryAllocator::reset()
{
    for (const auto& [_, memory] : m_memory_allocations)
    {
        vkFreeMemory(VulkanContext.device->handle(), memory, nullptr);
    }

    m_memory_allocations.clear();
    m_mapped_allocations.clear();
}

//
// VulkanAliasedDeviceMemoryAllocator
//

#define MIZU_RENDER_GRAPH_DEVICE_ALLOCATOR_DEBUG_ENABLED 0

VulkanAliasedDeviceMemoryAllocator::VulkanAliasedDeviceMemoryAllocator(bool host_visible, std::string name)
    : m_name(std::move(name))
{
    if (host_visible)
    {
        m_memory_property_flags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    }
    else
    {
        m_memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }
}

VulkanAliasedDeviceMemoryAllocator::~VulkanAliasedDeviceMemoryAllocator()
{
    free_if_allocated();
}

void VulkanAliasedDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer, size_t offset)
{
    const VulkanBufferResource& native_buffer = dynamic_cast<const VulkanBufferResource&>(buffer);

    VkMemoryRequirements reqs{};
    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), native_buffer.handle(), &reqs);

    BufferInfo info{};
    info.buffer = native_buffer.handle();
    info.usage = VulkanBufferResource::get_vulkan_usage(native_buffer.get_usage());
    info.size = reqs.size;
    info.offset = offset;
    info.memory_type_bits = reqs.memoryTypeBits;

    m_buffer_infos.push_back(info);
}

void VulkanAliasedDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image, size_t offset)
{
    const VulkanImageResource& native_image = dynamic_cast<const VulkanImageResource&>(image);

    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(VulkanContext.device->handle(), native_image.handle(), &reqs);

    ImageInfo info{};
    info.image = native_image.handle();
    info.usage = VulkanImageResource::get_vulkan_usage(native_image.get_usage(), native_image.get_format());
    info.size = reqs.size;
    info.offset = offset;
    info.memory_type_bits = reqs.memoryTypeBits;

    m_image_infos.push_back(info);
}

uint8_t* VulkanAliasedDeviceMemoryAllocator::get_mapped_memory() const
{
    MIZU_ASSERT(
        m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
        "Can't request mapped memory of a non host visible memory allocator");

    return static_cast<uint8_t*>(m_mapped_data);
}

void VulkanAliasedDeviceMemoryAllocator::allocate()
{
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

        if (info.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
        {
            memory_allocate_flags |= VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
        }
    }

    for (const ImageInfo& info : m_image_infos)
    {
        memory_type_bits &= info.memory_type_bits;
        max_size = std::max(info.offset + info.size, max_size);
    }

    const auto memory_index = VulkanContext.device->find_memory_type(memory_type_bits, m_memory_property_flags);
    MIZU_ASSERT(
        memory_index.has_value(),
        "Failed to find suitable memory type when allocating VulkanAliasedDeviceMemoryAllocator");

    if (m_memory == VK_NULL_HANDLE || max_size > m_size || *memory_index != m_memory_type_index)
    {
        m_size = max_size;
        m_memory_type_index = *memory_index;

        allocate_memory(m_size, memory_allocate_flags, m_memory_type_index);
    }

    bind_resources();

    m_buffer_infos.clear();
    m_image_infos.clear();
}

size_t VulkanAliasedDeviceMemoryAllocator::get_allocated_size() const
{
    return m_size;
}

void VulkanAliasedDeviceMemoryAllocator::bind_resources()
{
    for (const BufferInfo& info : m_buffer_infos)
    {
        VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), info.buffer, m_memory, info.offset));
    }

    for (const ImageInfo& info : m_image_infos)
    {
        VK_CHECK(vkBindImageMemory(VulkanContext.device->handle(), info.image, m_memory, info.offset));
    }
}

void VulkanAliasedDeviceMemoryAllocator::allocate_memory(
    size_t size,
    VkMemoryAllocateFlags allocate_flags,
    uint32_t memory_type_index)
{
    MIZU_PROFILE_SCOPED;

    Renderer::wait_idle();

    free_if_allocated();

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

    if (m_memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        VK_CHECK(vkMapMemory(VulkanContext.device->handle(), m_memory, 0, size, 0, &m_mapped_data));
    }
}

void VulkanAliasedDeviceMemoryAllocator::free_if_allocated()
{
    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
}

} // namespace Mizu::Vulkan
