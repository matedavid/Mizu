#pragma once

#include <vulkan/vulkan.h>

#include "render_core/rhi/resource_view.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;

class VulkanImageResourceView : public ImageResourceView
{
  public:
    VulkanImageResourceView(std::shared_ptr<ImageResource> resource, Range mip_range, Range layer_range);
    ~VulkanImageResourceView() override;

    [[nodiscard]] ImageFormat get_format() const override;
    [[nodiscard]] Range get_mip_range() const override;
    [[nodiscard]] Range get_layer_range() const override;

    // Added because vulkan_resource_group needs to know the usage of the image
    [[nodiscard]] ImageUsageBits get_image_usage() const;

    [[nodiscard]] static VkImageViewType get_vulkan_image_view_type(ImageType type);

    [[nodiscard]] VkImageView handle() const { return m_view; }

  private:
    VkImageView m_view;

    std::shared_ptr<VulkanImageResource> m_resource;
    Range m_mip_range, m_layer_range;
};

} // namespace Mizu::Vulkan