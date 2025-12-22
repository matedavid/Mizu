#pragma once

#include "render_core/rhi/resource_view.h"

#include "vulkan_core.h"

namespace Mizu::Vulkan
{

// Forward declarations
class VulkanImageResource;

struct VulkanBufferResourceView
{
    VkDeviceSize offset, size;
    VkBuffer handle;
};

struct VulkanImageResourceView
{
    ImageResourceViewDescription description;
    ImageFormat format;
    VkImageView handle;
};

VkImageView create_image_view(const ImageResourceViewDescription desc, const VulkanImageResource& resource);
void free_image_view(VkImageView view);

} // namespace Mizu::Vulkan