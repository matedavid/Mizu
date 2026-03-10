#pragma once

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/buffer_resource.h"
#include "render_core/rhi/device_memory_allocator.h"

#include "vulkan_core.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

class VulkanBufferResource : public BufferResource
{
  public:
    VulkanBufferResource(const BufferDescription& desc);
    ~VulkanBufferResource() override;

    ResourceView as_srv(const BufferResourceViewDescription& desc) override;
    ResourceView as_uav(const BufferResourceViewDescription& desc) override;
    ResourceView as_cbv(const BufferResourceViewDescription& desc) override;

    MemoryRequirements get_memory_requirements() const override;

    using BufferResource::set_data;

    uint8_t* get_mapped_data() const override { return m_mapped_data; }
    uint8_t* map() override;
    void unmap() override;

    uint64_t get_size() const override { return m_description.size; }
    uint64_t get_stride() const override { return m_description.stride; }
    BufferUsageBits get_usage() const override { return m_description.usage; }
    ResourceSharingMode get_sharing_mode() const override { return m_description.sharing_mode; }

    static VkBufferCreateInfo get_vulkan_buffer_create_info(
        const BufferDescription& desc,
        QueueFamiliesArray& queue_families);

    const std::string& get_name() const override { return m_description.name; }

    VkBuffer handle() const { return m_handle; }

  private:
    VkBuffer m_handle{VK_NULL_HANDLE};
    uint8_t* m_mapped_data = nullptr;

    // Considering 2 srvs, 2 uavs and 2 cbvs at max
    static constexpr size_t MAX_RESOURCE_VIEWS = 6;
    inplace_vector<ResourceView, MAX_RESOURCE_VIEWS> m_resource_views;

    ResourceView get_or_create_resource_view(ResourceViewType type, const BufferResourceViewDescription& desc);

    BufferDescription m_description{};
    AllocationInfo m_allocation_info{};
};

MemoryRequirements get_vulkan_buffer_memory_requirements(const BufferDescription& desc);

} // namespace Mizu::Vulkan