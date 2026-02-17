#pragma once

#include "base/containers/inplace_vector.h"
#include "render_core/rhi/device_memory_allocator.h"
#include "render_core/rhi/image_resource.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

class VulkanImageResource : public ImageResource
{
  public:
    VulkanImageResource(const ImageDescription& desc);
    // Only used by VulkanSwapchain
    VulkanImageResource(
        uint32_t width,
        uint32_t height,
        ImageFormat format,
        ImageUsageBits usage,
        VkImage image,
        bool owns_resources);
    ~VulkanImageResource() override;

    ResourceView as_srv(ImageResourceViewDescription desc = {}) override;
    ResourceView as_uav(ImageResourceViewDescription desc = {}) override;
    ResourceView as_rtv(ImageResourceViewDescription desc = {}) override;

    MemoryRequirements get_memory_requirements() const override;
    ImageMemoryRequirements get_image_memory_requirements() const override;

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }
    uint32_t get_depth() const override { return m_description.depth; }
    ImageType get_image_type() const override { return m_description.type; }
    ImageFormat get_format() const override { return m_description.format; }
    ImageUsageBits get_usage() const override { return m_description.usage; }
    ResourceSharingMode get_sharing_mode() const override { return m_description.sharing_mode; }
    uint32_t get_num_mips() const override { return m_description.num_mips; }
    uint32_t get_num_layers() const override { return m_description.num_layers; }

    std::string_view get_name() const override { return m_description.name; }

    static VkImageType get_vulkan_image_type(ImageType type);
    static VkFormat get_vulkan_image_format(ImageFormat format);
    static VkImageLayout get_vulkan_image_resource_state(ImageResourceState state);

    static VkImageUsageFlags get_vulkan_usage(ImageUsageBits usage, ImageFormat format);
    VkImageFormatProperties get_format_properties() const;

    VkImage handle() const { return m_handle; }

  private:
    VkImage m_handle{VK_NULL_HANDLE};

    // Considering 2 srvs, 2 uavs and 4 rtvs at max
    static constexpr size_t MAX_RESOURCE_VIEWS = 8;
    inplace_vector<ResourceView, MAX_RESOURCE_VIEWS> m_resource_views;

    ResourceView get_or_create_resource_view(ResourceViewType type, const ImageResourceViewDescription& desc);

    ImageDescription m_description;
    AllocationInfo m_allocation_info{};

    bool m_owns_resources = true;
};

} // namespace Mizu::Vulkan