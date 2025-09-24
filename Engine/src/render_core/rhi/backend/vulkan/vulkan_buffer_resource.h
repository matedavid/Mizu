#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"

namespace Mizu::Vulkan
{

class VulkanBufferResource : public BufferResource
{
  public:
    VulkanBufferResource(const BufferDescription& desc);
    ~VulkanBufferResource() override;

    MemoryRequirements get_memory_requirements() const override;

    void set_data(const uint8_t* data) const override;

    static VkBufferUsageFlags get_vulkan_usage(BufferUsageBits usage);

    uint64_t get_size() const override { return m_description.size; }
    BufferUsageBits get_usage() const override { return m_description.usage; }

    VkBuffer handle() const { return m_handle; }

  private:
    VkBuffer m_handle{VK_NULL_HANDLE};
    BufferDescription m_description{};

    AllocationInfo m_allocation_info{};
};

} // namespace Mizu::Vulkan