#include "vulkan_buffer_resource.h"

#include <cstring>
#include <utility>

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

#include "utility/assert.h"

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

    if (m_description.type == BufferType::UniformBuffer || m_description.type == BufferType::StorageBuffer
        || m_description.type == BufferType::Staging)
    {
        VK_CHECK(vkMapMemory(VulkanContext.device->handle(), memory, offset, m_description.size, 0, &m_mapped_data));
    }
}

VulkanBufferResource::VulkanBufferResource(const BufferDescription& desc,
                                           const uint8_t* data,
                                           std::weak_ptr<IDeviceMemoryAllocator> allocator)
    : VulkanBufferResource(desc, std::move(allocator))
{
    BufferDescription staging_desc{};
    staging_desc.size = m_description.size;
    staging_desc.type = BufferType::Staging;

    const VulkanBufferResource staging_buffer(staging_desc, m_allocator);
    staging_buffer.set_data(data);

    staging_buffer.copy_to_buffer(*this);
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
    create_info.usage = get_vulkan_usage(m_description.type);
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

void VulkanBufferResource::copy_to_buffer(const VulkanBufferResource& buffer) const
{
    MIZU_ASSERT(get_size() == buffer.get_size(), "Size of buffers do not match");

    VulkanTransferCommandBuffer::submit_single_time([&](const VulkanTransferCommandBuffer& command_buffer) {
        VkBufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = get_size();

        vkCmdCopyBuffer(command_buffer.handle(), m_handle, buffer.handle(), 1, &copy);
    });
}

void VulkanBufferResource::copy_to_image(const VulkanImageResource& image) const
{
    VulkanTransferCommandBuffer::submit_single_time([&](const VulkanTransferCommandBuffer& command_buffer) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = image.get_num_layers();

        region.imageOffset = {0, 0, 0};
        region.imageExtent = {image.get_width(), image.get_height(), 1};

        vkCmdCopyBufferToImage(command_buffer.handle(),
                               m_handle,
                               image.get_image_handle(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &region);
    });
}

VkBufferUsageFlags VulkanBufferResource::get_vulkan_usage(BufferType type)
{
    VkBufferUsageFlags vulkan_usage = 0;

    switch (type)
    {
    case BufferType::VertexBuffer:
        vulkan_usage |= (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        break;
    case BufferType::IndexBuffer:
        vulkan_usage |= (VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        break;
    case BufferType::UniformBuffer:
        vulkan_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        break;
    case BufferType::StorageBuffer:
        vulkan_usage |= (VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        break;
    case BufferType::Staging:
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        break;
    }

    return vulkan_usage;
}

} // namespace Mizu::Vulkan