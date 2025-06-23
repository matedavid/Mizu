#include "vulkan_buffer_resource.h"

#include <cstring>
#include <utility>

#include "base/debug/assert.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

namespace Mizu::Vulkan
{

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc) : m_description(desc) {}

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc,
                                           std::weak_ptr<IDeviceMemoryAllocator> allocator)
    : m_description(desc)
    , m_allocator(std::move(allocator))
{
    create_buffer();

    VkDeviceMemory memory{VK_NULL_HANDLE};
    size_t offset = 0;

    if (std::shared_ptr<IDeviceMemoryAllocator> tmp_allocator = m_allocator.lock())
    {
        m_allocation = tmp_allocator->allocate_buffer_resource(*this);

        const auto& native_allocator = std::dynamic_pointer_cast<IVulkanDeviceMemoryAllocator>(tmp_allocator);

        const VulkanAllocationInfo& info = native_allocator->get_allocation_info(m_allocation);
        memory = info.memory;
        offset = info.offset;
    }
    else
    {
        MIZU_UNREACHABLE("Failed to allocate buffer resource");
    }

    if (m_description.usage & BufferUsageBits::HostVisible)
    {
        VK_CHECK(vkMapMemory(VulkanContext.device->handle(), memory, offset, m_description.size, 0, &m_mapped_data));
    }
}

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc,
                                           const uint8_t* data,
                                           std::weak_ptr<IDeviceMemoryAllocator> allocator)
    : VulkanBufferResource(desc, std::move(allocator))
{
    if (m_description.usage & BufferUsageBits::HostVisible)
    {
        set_data(data);
    }
    else
    {
        BufferDescription staging_desc{};
        staging_desc.size = m_description.size;
        staging_desc.usage = BufferUsageBits::HostVisible | BufferUsageBits::TransferSrc;

        const VulkanBufferResource staging_buffer(staging_desc, m_allocator);
        staging_buffer.set_data(data);

        TransferCommandBuffer::submit_single_time(
            [&](CommandBuffer& command) { command.copy_buffer_to_buffer(staging_buffer, *this); });
    }
}

VulkanBufferResource::~VulkanBufferResource()
{
    if (std::shared_ptr<IDeviceMemoryAllocator> allocator = m_allocator.lock())
    {
        allocator->release(m_allocation);
    }

    vkDestroyBuffer(VulkanContext.device->handle(), m_handle, nullptr);
}

void VulkanBufferResource::create_buffer()
{
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = m_description.size;
    create_info.usage = get_vulkan_usage(m_description.usage);
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &create_info, nullptr, &m_handle));

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_handle, m_description.name);
    }
}

void VulkanBufferResource::set_data(const uint8_t* data) const
{
    MIZU_ASSERT(m_mapped_data != nullptr, "Memory is not mapped");
    memcpy(m_mapped_data, data, m_description.size);
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
        vulkan_usage |= (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
                         | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxShaderBindingTable)
        vulkan_usage |= (VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    return vulkan_usage;
}

} // namespace Mizu::Vulkan