#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;

class VulkanBufferResource : public BufferResource
{
  public:
    // NOTE: Should only be used by VulkanTransientBufferResource
    VulkanBufferResource(const BufferDescription& desc);

    VulkanBufferResource(const BufferDescription& desc, std::weak_ptr<IDeviceMemoryAllocator> allocator);

    VulkanBufferResource(const BufferDescription& desc,
                         const uint8_t* data,
                         std::weak_ptr<IDeviceMemoryAllocator> allocator);

    ~VulkanBufferResource() override;

    void create_buffer();

    void set_data(const uint8_t* data) const override;
    void copy_to_buffer(const VulkanBufferResource& buffer) const;
    void copy_to_image(const VulkanImageResource& image) const;

    [[nodiscard]] size_t get_size() const override { return m_description.size; }
    [[nodiscard]] BufferType get_type() const override { return m_description.type; }

    [[nodiscard]] VkBuffer handle() const { return m_handle; }

  private:
    VkBuffer m_handle{VK_NULL_HANDLE};
    BufferDescription m_description;

    void* m_mapped_data{nullptr};

    std::weak_ptr<IDeviceMemoryAllocator> m_allocator;
    Allocation m_allocation = Allocation::invalid();

    [[nodiscard]] static VkBufferUsageFlags get_vulkan_usage(BufferType type);
};

} // namespace Mizu::Vulkan