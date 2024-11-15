#include "vulkan_buffer.h"

#include <cstring>

#include "utility/logging.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image_resource.h"

namespace Mizu::Vulkan {

VulkanBuffer::VulkanBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
      : m_size(size) {
    // Create buffer
    VkBufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    create_info.size = size;
    create_info.usage = usage;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(VulkanContext.device->handle(), &create_info, nullptr, &m_buffer));

    // Allocate memory
    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(VulkanContext.device->handle(), m_buffer, &memory_requirements);

    const auto memory_type_index =
        VulkanContext.device->find_memory_type(memory_requirements.memoryTypeBits, properties);
    assert(memory_type_index.has_value() && "No suitable memory to allocate buffer");

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = memory_type_index.value();

    VK_CHECK(vkAllocateMemory(VulkanContext.device->handle(), &allocate_info, nullptr, &m_memory));

    // Bind memory to buffer
    VK_CHECK(vkBindBufferMemory(VulkanContext.device->handle(), m_buffer, m_memory, 0));
}

VulkanBuffer::~VulkanBuffer() {
    vkDestroyBuffer(VulkanContext.device->handle(), m_buffer, nullptr);
    vkFreeMemory(VulkanContext.device->handle(), m_memory, nullptr);
}

void VulkanBuffer::map_memory(void*& memory) const {
    VK_CHECK(vkMapMemory(VulkanContext.device->handle(), m_memory, 0, m_size, 0, &memory));
}

void VulkanBuffer::unmap_memory() const {
    vkUnmapMemory(VulkanContext.device->handle(), m_memory);
}

void VulkanBuffer::copy_data(const void* data) const {
    void* mem;
    map_memory(mem);
    memcpy(mem, data, m_size);
    unmap_memory();
}

void VulkanBuffer::copy_to_buffer(const VulkanBuffer& buffer) const {
    assert(m_size == buffer.m_size && "Size of buffers do not match");

    VulkanTransferCommandBuffer::submit_single_time([&](const VulkanTransferCommandBuffer& command_buffer) {
        VkBufferCopy copy{};
        copy.srcOffset = 0;
        copy.dstOffset = 0;
        copy.size = m_size;

        vkCmdCopyBuffer(command_buffer.handle(), m_buffer, buffer.handle(), 1, &copy);
    });
}

void VulkanBuffer::copy_to_image(const VulkanImageResource& image) const {
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
                               m_buffer,
                               image.get_image_handle(),
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               1,
                               &region);
    });
}

} // namespace Mizu::Vulkan