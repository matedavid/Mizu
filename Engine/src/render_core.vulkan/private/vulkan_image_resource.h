#pragma once

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

    MemoryRequirements get_memory_requirements() const override;
    ImageMemoryRequirements get_image_memory_requirements() const override;

    uint32_t get_width() const override { return m_description.width; }
    uint32_t get_height() const override { return m_description.height; }
    uint32_t get_depth() const override { return m_description.depth; }
    ImageType get_image_type() const override { return m_description.type; }
    ImageFormat get_format() const override { return m_description.format; }
    ImageUsageBits get_usage() const override { return m_description.usage; }
    uint32_t get_num_mips() const override { return m_description.num_mips; }
    uint32_t get_num_layers() const override { return m_description.num_layers; }

    const std::string& get_name() const override { return m_description.name; }

    static VkImageType get_vulkan_image_type(ImageType type);
    static VkFormat get_vulkan_image_format(ImageFormat format);
    static VkImageLayout get_vulkan_image_resource_state(ImageResourceState state);

    static VkImageUsageFlags get_vulkan_usage(ImageUsageBits usage, ImageFormat format);
    VkImageFormatProperties get_format_properties() const;

    VkImage handle() const { return m_handle; }

  private:
    VkImage m_handle{VK_NULL_HANDLE};

    ImageDescription m_description;
    AllocationInfo m_allocation_info{};

    bool m_owns_resources = true;
};

} // namespace Mizu::Vulkan