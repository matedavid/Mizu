#pragma once

#include "render_core/rhi/buffer_resource.h"

#include "vulkan_core.h"
#include "vulkan_gpu_heap.h"
#include "vulkan_resource_view.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

class VulkanBufferResource : public BufferResource
{
  public:
    VulkanBufferResource(const BufferDescription& desc);
    VulkanBufferResource(const BufferDescription& desc, const MemoryPlacement& placement);
    VulkanBufferResource(const SparseBufferDescription& desc);
    ~VulkanBufferResource() override;

    VulkanBufferResourceView as_srv(const BufferResourceViewDescription& desc);
    VulkanBufferResourceView as_uav(const BufferResourceViewDescription& desc);
    VulkanBufferResourceView as_cbv(const BufferResourceViewDescription& desc);

    using BufferResource::set_data;
    uint8_t* get_mapped_memory() const override;

    MemoryRequirements get_memory_requirements() const override;

    const BufferDescription& get_description() const override { return m_description; }
    bool is_sparse() const override { return m_sparse; }

    VkBuffer handle() const { return m_handle; }

  private:
    VkBuffer m_handle{VK_NULL_HANDLE};
    bool m_sparse = false;
    uint8_t* m_mapped_memory = nullptr;

    BufferDescription m_description{};

    VulkanBufferResourceView get_or_create_resource_view(
        ResourceViewType type,
        const BufferResourceViewDescription& desc);
};

VkBufferCreateInfo get_vulkan_buffer_create_info(const BufferDescription& desc, QueueFamiliesArray& queue_families);
MemoryRequirements get_vulkan_buffer_memory_requirements(const BufferDescription& desc);

} // namespace Mizu::Vulkan