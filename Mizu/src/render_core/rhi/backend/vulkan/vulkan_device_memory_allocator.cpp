#include "vulkan_device_memory_allocator.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

#include "utility/assert.h"
#include "utility/logging.h"

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
        MIZU_LOG_ERROR("Some memory chunks were not released manually ({}), this could cause problems",
                       m_memory_allocations.size());
    }
#endif

    for (const auto& [_, memory] : m_memory_allocations)
    {
        vkFreeMemory(VulkanContext.device->handle(), memory, nullptr);
    }

    m_memory_allocations.clear();
}

Allocation VulkanBaseDeviceMemoryAllocator::allocate_buffer_resource(const BufferResource& buffer)
{
    const auto& native_buffer = dynamic_cast<const VulkanBufferResource&>(buffer);

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), native_buffer.handle(), &memory_requirements);

    VkMemoryPropertyFlags memory_property_flags = 0;
    {
        const BufferType& type = native_buffer.get_type();
        if (type == BufferType::VertexBuffer || type == BufferType::IndexBuffer)
        {
            memory_property_flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }

        if (type == BufferType::UniformBuffer || type == BufferType::StorageBuffer || type == BufferType::Staging)
        {
            memory_property_flags |= (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        }
    }

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, memory_property_flags);
    MIZU_ASSERT(memory_type_index.has_value(), "No suitable memory to allocate image");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = *memory_type_index;

    VkDeviceMemory memory;

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), native_buffer.handle(), memory, 0));

    const auto id = Allocation();
    m_memory_allocations.insert({id, memory});

    return id;
}

Allocation VulkanBaseDeviceMemoryAllocator::allocate_image_resource(const ImageResource& image)
{
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

void VulkanBaseDeviceMemoryAllocator::release(Allocation id)
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

VulkanAllocationInfo VulkanBaseDeviceMemoryAllocator::get_allocation_info(Allocation id) const
{
    const auto it = m_memory_allocations.find(id);
    if (it == m_memory_allocations.end())
    {
        MIZU_LOG_WARNING("Allocation {} does not exist", static_cast<UUID::Type>(id));
        return VulkanAllocationInfo{};
    }

    return VulkanAllocationInfo{
        .memory = it->second,
        .offset = 0,
    };
}

//
// VulkanRenderGraphDeviceMemoryAllocator
//

#define MIZU_RENDER_GRAPH_DEVICE_ALLOCATOR_DEBUG_ENABLED 0

VulkanTransientImageResource::VulkanTransientImageResource(const ImageDescription& desc,
                                                           const SamplingOptions& sampling)
{
    m_resource = std::make_shared<VulkanImageResource>(desc, sampling, true);
    m_resource->create_image();

    vkGetImageMemoryRequirements(VulkanContext.device->handle(), m_resource->get_image_handle(), &m_memory_reqs);
}

VulkanTransientBufferResource::VulkanTransientBufferResource(const BufferDescription& desc)
{
    m_resource = std::make_shared<VulkanBufferResource>(desc);
    m_resource->create_buffer();

    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), m_resource->handle(), &m_memory_reqs);
}

VulkanTransientBufferResource::VulkanTransientBufferResource(const BufferDescription& desc,
                                                             const std::vector<uint8_t>& data)
{
    MIZU_ASSERT(desc.size == data.size(), "Provided size and data size do not match");

    m_resource = std::make_shared<VulkanBufferResource>(desc);
    m_resource->create_buffer();

    m_buffer_data = data.data();

    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), m_resource->handle(), &m_memory_reqs);
}

VulkanRenderGraphDeviceMemoryAllocator::~VulkanRenderGraphDeviceMemoryAllocator()
{
    free_if_allocated();
}

void VulkanRenderGraphDeviceMemoryAllocator::allocate_image_resource(const TransientImageResource& resource,
                                                                     size_t offset)
{
    const auto& native_transient = dynamic_cast<const VulkanTransientImageResource&>(resource);
    const auto& native_resource = std::dynamic_pointer_cast<VulkanImageResource>(resource.get_resource());

    ImageAllocationInfo info{};
    info.image = native_resource;
    info.memory_type_bits = native_transient.get_memory_requirements().memoryTypeBits;
    info.size = resource.get_size();
    info.offset = offset;

    m_image_allocations.push_back(info);
}

void VulkanRenderGraphDeviceMemoryAllocator::allocate_buffer_resource(const TransientBufferResource& resource,
                                                                      size_t offset)
{
    const auto& native_transient = dynamic_cast<const VulkanTransientBufferResource&>(resource);
    const auto& native_resource = std::dynamic_pointer_cast<VulkanBufferResource>(resource.get_resource());

    BufferAllocationInfo info{};
    info.buffer = native_resource;
    info.memory_type_bits = native_transient.get_memory_requirements().memoryTypeBits;
    info.size = resource.get_size();
    info.offset = offset;
    info.data = native_transient.get_data();

    m_buffer_allocations.push_back(info);
}

void VulkanRenderGraphDeviceMemoryAllocator::allocate()
{
    if (m_image_allocations.empty() && m_buffer_allocations.empty())
    {
        free_if_allocated();
        m_size = 0;

        return;
    }

    uint32_t memory_type_bits = 0;
    uint64_t max_size = 0;

    for (const ImageAllocationInfo& info : m_image_allocations)
    {
        memory_type_bits |= info.memory_type_bits;

        if (info.offset + info.size > max_size)
        {
            max_size = info.offset + info.size;
        }
    }

    for (const BufferAllocationInfo& info : m_buffer_allocations)
    {
        memory_type_bits |= info.memory_type_bits;

        if (info.offset + info.size > max_size)
        {
            max_size = info.offset + info.size;
        }
    }

    const std::optional<uint32_t> memory_type_index =
        VulkanContext.device->find_memory_type(memory_type_bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    MIZU_ASSERT(memory_type_index.has_value(), "No memory type found");

    if (m_memory == VK_NULL_HANDLE || m_size != max_size)
    {
        free_if_allocated();

        m_size = max_size;

        VkMemoryAllocateInfo allocate_info{};
        allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocate_info.allocationSize = m_size;
        allocate_info.memoryTypeIndex = *memory_type_index;

        VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));
    }

#if MIZU_RENDER_GRAPH_DEVICE_ALLOCATOR_DEBUG_ENABLED
    uint32_t non_aliased_size = 0;

    for (const ImageAllocationInfo& info : m_image_allocations)
    {
        non_aliased_size += info.size;
    }

    for (const BufferAllocationInfo& info : m_buffer_allocations)
    {
        non_aliased_size += info.size;
    }

    MIZU_LOG_INFO("Aliased memory total size: {}", m_size);
    MIZU_LOG_INFO("Non Aliased memory total size: {}", non_aliased_size);
#endif

    bind_resources();

    // Clear resources
    m_image_allocations.clear();
    m_buffer_allocations.clear();
}

void VulkanRenderGraphDeviceMemoryAllocator::bind_resources()
{
    for (const ImageAllocationInfo& info : m_image_allocations)
    {
        VK_CHECK(
            vkBindImageMemory(VulkanContext.device->handle(), info.image->get_image_handle(), m_memory, info.offset));

        info.image->create_image_views();
        info.image->create_sampler();
    }

    for (const BufferAllocationInfo& info : m_buffer_allocations)
    {
        VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), info.buffer->handle(), m_memory, info.offset));

        if (info.data != nullptr)
        {
            BufferDescription staging_desc{};
            staging_desc.size = info.size;
            staging_desc.type = BufferType::Staging;

            const VulkanBufferResource staging_buffer(staging_desc, Renderer::get_allocator());
            staging_buffer.set_data(info.data);

            staging_buffer.copy_to_buffer(*info.buffer);
        }
    }
}

void VulkanRenderGraphDeviceMemoryAllocator::free_if_allocated()
{
    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
    }
}

} // namespace Mizu::Vulkan
