#pragma once

#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"

#include "vulkan_core.h"
#include "vulkan_resource_view.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

class VulkanBufferResource : public BufferResource
{
  public:
    VulkanBufferResource(const BufferDescription& desc);
    ~VulkanBufferResource() override;

    VulkanBufferResourceView as_srv(const BufferResourceViewDescription& desc);
    VulkanBufferResourceView as_uav(const BufferResourceViewDescription& desc);
    VulkanBufferResourceView as_cbv(const BufferResourceViewDescription& desc);

    MemoryRequirements get_memory_requirements() const override;

    using BufferResource::set_data;

    uint8_t* get_mapped_data() const override { return m_mapped_data; }
    uint8_t* map() override;
    void unmap() override;

    const BufferDescription& get_description() const override { return m_description; }

    static VkBufferCreateInfo get_vulkan_buffer_create_info(
        const BufferDescription& desc,
        QueueFamiliesArray& queue_families);

    VkBuffer handle() const { return m_handle; }

    void set_allocation_info(const AllocationInfo& info) { m_allocation_info = info; }
    const AllocationInfo& get_allocation_info() const { return m_allocation_info; }

  private:
    VkBuffer m_handle{VK_NULL_HANDLE};
    uint8_t* m_mapped_data = nullptr;

    VulkanBufferResourceView get_or_create_resource_view(
        ResourceViewType type,
        const BufferResourceViewDescription& desc);

    BufferDescription m_description{};
    AllocationInfo m_allocation_info{};
};

MemoryRequirements get_vulkan_buffer_memory_requirements(const BufferDescription& desc);

} // namespace Mizu::Vulkan