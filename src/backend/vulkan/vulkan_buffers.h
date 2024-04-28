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
    VulkanVertexBuffer(const void* data, uint32_t count, uint32_t size);
    ~VulkanVertexBuffer() override = default;

    void bind(const std::shared_ptr<ICommandBuffer>& command_buffer) const override;
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
