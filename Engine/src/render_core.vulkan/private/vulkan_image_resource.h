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
    struct SparseTag
    {
    };
    static constexpr SparseTag Sparse{};

    VulkanImageResource(const ImageDescription& desc, const MemoryPlacement& placement);
    VulkanImageResource(const ImageDescription& desc, SparseTag);
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
    SparseImageMemoryRequirements get_sparse_memory_requirements() const override;

    const ImageDescription& get_description() const override { return m_description; }
    bool is_sparse() const override { return m_sparse; }

    uint64_t get_mip_tail_resource_offset(uint32_t array_layer) const;
    uint32_t get_metadata_region_count() const { return m_metadata_region_count; }
    uint64_t get_metadata_region_size() const { return m_metadata_region_size; }
    uint64_t get_metadata_resource_offset(uint32_t region) const;

    VkImage handle() const { return m_handle; }

  private:
    VkImage m_handle{VK_NULL_HANDLE};
    bool m_sparse = false;

    MemoryRequirements m_memory_requirements{};
    ImageMemoryRequirements m_image_memory_requirements{};

    SparseImageMemoryRequirements m_sparse_requirements{};
    uint64_t m_mip_tail_offset = 0;
    uint64_t m_mip_tail_stride = 0;

    uint64_t m_metadata_offset = 0;
    uint64_t m_metadata_stride = 0;
    uint64_t m_metadata_region_size = 0;
    uint32_t m_metadata_region_count = 0;

    ImageDescription m_description{};
    bool m_owns_resources = true;

    std::unordered_map<size_t, VulkanImageResourceView> m_resource_views;

    void init_memory_requirements();
    VulkanImageResourceView get_or_create_resource_view(
        ResourceViewType type,
        const ImageResourceViewDescription& desc);
};

VkImageCreateInfo get_vulkan_image_create_info(const ImageDescription& desc, QueueFamiliesArray& queue_families);
MemoryRequirements get_vulkan_image_memory_requirements(const ImageDescription& desc);

} // namespace Mizu::Vulkan