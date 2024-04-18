#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

#include "buffers.h"
#include "command_buffer.h"

#include "backend/vulkan/vulkan_buffer.h"
#include "backend/vulkan/vulkan_command_buffer.h"
#include "backend/vulkan/vulkan_context.h"

namespace Mizu::Vulkan {

class VulkanVertexBuffer : public VertexBuffer {
  public:
    template <typename T>
    explicit VulkanVertexBuffer(const std::vector<T>& data) {
        const VkDeviceSize size = data.size() * sizeof(T);

        const auto staging_buffer = VulkanBuffer{
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        staging_buffer.copy_data(data.data());

        m_buffer = std::make_unique<VulkanBuffer>(size,
                                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        staging_buffer.copy_to_buffer(*m_buffer);

        m_count = static_cast<uint32_t>(data.size());
    }

    ~VulkanVertexBuffer() override = default;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override {
        const auto& native = dynamic_pointer_cast<IVulkanCommandBuffer>(command_buffer);

        const std::array<VkBuffer, 1> vertex_buffers = {m_buffer->handle()};
        const VkDeviceSize offsets[] = {0};

        vkCmdBindVertexBuffers(native->handle(), 0, vertex_buffers.size(), vertex_buffers.data(), offsets);
    }

    [[nodiscard]] uint32_t count() const override { return m_count; }

    [[nodiscard]] VkBuffer handle() const { return m_buffer->handle(); }

  private:
    std::unique_ptr<VulkanBuffer> m_buffer;
    uint32_t m_count;
};

class VulkanIndexBuffer : public IndexBuffer {
  public:
    explicit VulkanIndexBuffer(const std::vector<uint32_t>& indices);
    ~VulkanIndexBuffer() override = default;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
    [[nodiscard]] uint32_t count() const override { return m_count; }

    [[nodiscard]] VkBuffer handle() const { return m_buffer->handle(); }

  private:
    std::unique_ptr<VulkanBuffer> m_buffer;
    uint32_t m_count;
};

class VulkanUniformBuffer : public UniformBuffer {
  public:
    explicit VulkanUniformBuffer(uint32_t size);
    ~VulkanUniformBuffer() override;

    template <typename T>
    static std::shared_ptr<VulkanUniformBuffer> create() {
        const uint32_t size = sizeof(T);
        return std::make_shared<VulkanUniformBuffer>(size);
    }

    void set_data(const void* data) override;
    void set_data(const void* data, uint32_t size, uint32_t offset_bytes) override;

    [[nodiscard]] uint32_t size() const override { return m_size; }
    [[nodiscard]] VkBuffer handle() const { return m_buffer->handle(); }

  private:
    std::unique_ptr<VulkanBuffer> m_buffer;

    void* m_map_data{nullptr};
    uint32_t m_size;
};

} // namespace Mizu::Vulkan
