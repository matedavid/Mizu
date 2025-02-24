#pragma once

#include "render_core/rhi/resource_view.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"

namespace Mizu::Vulkan
{

class VulkanImageResourceView : public ImageResourceView
{
  public:
    VulkanImageResourceView(const ImageResource& resource, Range mip_range, Range layer_range);
    ~VulkanImageResourceView() override;

    [[nodiscard]] VkImageView handle() const { return m_view; }

  private:
    VkImageView m_view;
};

} // namespace Mizu::Vulkan