#pragma once

#include <unordered_map>

#include "render_core/rhi/image_resource.h"

#include "vulkan_core.h"
#include "vulkan_gpu_heap.h"
#include "vulkan_resource_view.h"
#include "vulkan_types.h"

namespace Mizu::Vulkan
{

class VulkanImageResource : public ImageResource
{
  public:
    VulkanImageResource(const ImageDescription& desc);
    VulkanImageResource(const ImageDescription& desc, const MemoryPlacement& placement);
    VulkanImageResource(const SparseImageDescription& desc);
    // Only used by VulkanSwapchain
    VulkanImageResource(
        uint32_t width,
        uint32_t height,
        ImageFormat format,
        ImageUsageBits usage,
        ImageFlagBits flags,
        VkImage image,
        bool owns_resources);
    ~VulkanImageResource() override;

    VulkanImageResourceView as_srv(const ImageResourceViewDescription& desc);
    VulkanImageResourceView as_uav(const ImageResourceViewDescription& desc);
    VulkanImageResourceView as_rtv(const ImageResourceViewDescription& desc);

    MemoryRequirements get_memory_requirements() const override;
    ImageMemoryRequirements get_image_memory_requirements() const override;

    const ImageDescription& get_description() const override { return m_description; }
    bool is_sparse() const override { return m_sparse; }

    VkImage handle() const { return m_handle; }

  private:
    VkImage m_handle{VK_NULL_HANDLE};
    bool m_sparse = false;

    ImageDescription m_description{};
    bool m_owns_resources = true;

    std::unordered_map<size_t, VulkanImageResourceView> m_resource_views;

    VulkanImageResourceView get_or_create_resource_view(
        ResourceViewType type,
        const ImageResourceViewDescription& desc);
};

VkImageCreateInfo get_vulkan_image_create_info(const ImageDescription& desc, QueueFamiliesArray& queue_families);
MemoryRequirements get_vulkan_image_memory_requirements(const ImageDescription& desc);

} // namespace Mizu::Vulkan