#include "vulkan_buffers.h"

#include <cstring>
#include <array>

namespace Mizu::Vulkan {

//
// VulkanVertexBuffer
//

VulkanVertexBuffer::VulkanVertexBuffer(const void* data, uint32_t count, uint32_t size) : m_count(count) {
    const auto staging_buffer = VulkanBuffer{
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    staging_buffer.copy_data(data);

    m_buffer = std::make_unique<VulkanBuffer>(size,
                                              VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    staging_buffer.copy_to_buffer(*m_buffer);
}
void VulkanVertexBuffer::bind(VkCommandBuffer command_buffer) const {
    const std::array<VkBuffer, 1> vertex_buffers = {m_buffer->handle()};
    const VkDeviceSize offsets[] = {0};

    vkCmdBindVertexBuffers(command_buffer, 0, vertex_buffers.size(), vertex_buffers.data(), offsets);
}

//
// VulkanIndexBuffer
//

VulkanIndexBuffer::VulkanIndexBuffer(const std::vector<uint32_t>& indices) : m_count((uint32_t)indices.size()) {
    const VkDeviceSize size = indices.size() * sizeof(uint32_t);

    const auto staging_buffer = VulkanBuffer{
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };

    staging_buffer.copy_data(indices.data());

    m_buffer = std::make_unique<VulkanBuffer>(
        size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    staging_buffer.copy_to_buffer(*m_buffer);
}

void VulkanIndexBuffer::bind(VkCommandBuffer command_buffer) const {
    vkCmdBindIndexBuffer(command_buffer, m_buffer->handle(), 0, VK_INDEX_TYPE_UINT32);
}

//
// Uniform Buffer
//

VulkanUniformBuffer::VulkanUniformBuffer(uint32_t size) : m_size(size) {
    m_buffer =
        std::make_unique<VulkanBuffer>(m_size,
                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    m_buffer->map_memory(m_map_data);
}

VulkanUniformBuffer::~VulkanUniformBuffer() {
    m_buffer->unmap_memory();
}

void VulkanUniformBuffer::set_data(const void* data) {
    memcpy(m_map_data, data, m_size);
}

void VulkanUniformBuffer::set_data(const void* data, uint32_t size, uint32_t offset_bytes) {
    auto* d = static_cast<char*>(m_map_data);
    memcpy(d + offset_bytes, data, size);
}

} // namespace Mizu::Vulkan
