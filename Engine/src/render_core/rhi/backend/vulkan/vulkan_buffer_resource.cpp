#include "vulkan_buffer_resource.h"

#include <cstring>

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu::Vulkan
{

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc) : m_description(desc)
{
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = m_description.size;
    create_info.usage = get_vulkan_usage(m_description.usage);
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    MIZU_ASSERT(create_info.usage != 0, "Failed to create buffer '{}', no usage was specified", m_description.name);

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    if (!m_description.is_virtual)
    {
        m_allocation_info = Renderer::get_allocator()->allocate_buffer_resource(*this);
    }

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

VulkanBufferResource::~VulkanBufferResource()
{
    if (!m_description.is_virtual)
    {
        Renderer::get_allocator()->release(m_allocation_info.id);
    }

    vkDestroyBuffer(VulkanContext.device->handle(), m_handle, nullptr);
}

MemoryRequirements VulkanBufferResource::get_memory_requirements() const
{
    VkBufferMemoryRequirementsInfo2 vk_reqs_info2{};
    vk_reqs_info2.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2;
    vk_reqs_info2.buffer = m_handle;

    VkMemoryDedicatedRequirements dedicated_reqs{};
    dedicated_reqs.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS;

    VkMemoryRequirements2 vk_reqs2{};
    vk_reqs2.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vk_reqs2.pNext = &dedicated_reqs;

    vkGetBufferMemoryRequirements2(VulkanContext.device->handle(), &vk_reqs_info2, &vk_reqs2);

    MemoryRequirements reqs{};
    reqs.size = vk_reqs2.memoryRequirements.size;
    reqs.alignment = vk_reqs2.memoryRequirements.alignment;

    return reqs;
}

void VulkanBufferResource::set_data(const uint8_t* data, size_t size, size_t offset) const
{
    MIZU_ASSERT(
        m_description.usage & BufferUsageBits::HostVisible, "Can't map data that does not have the HostVisible usage");

    uint8_t* mapped = Renderer::get_allocator()->get_mapped_memory(m_allocation_info.id);
    MIZU_ASSERT(mapped != nullptr, "Memory is not mapped");

    memcpy(mapped + m_allocation_info.offset + offset, data, size);
}

VkBufferUsageFlags VulkanBufferResource::get_vulkan_usage(BufferUsageBits usage)
{
    VkBufferUsageFlags vulkan_usage = 0;

    if (usage & BufferUsageBits::VertexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & BufferUsageBits::IndexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & BufferUsageBits::UniformBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & BufferUsageBits::StorageBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & BufferUsageBits::TransferSrc)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & BufferUsageBits::TransferDst)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & BufferUsageBits::RtxAccelerationStructureStorage)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxAccelerationStructureInputReadOnly)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxShaderBindingTable)
        vulkan_usage |= (VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    return vulkan_usage;
}

} // namespace Mizu::Vulkan