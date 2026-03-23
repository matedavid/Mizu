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
    uint32_t stride;
    ResourceViewType type;
    VkBuffer handle;
};

struct VulkanImageResourceView
{
    ImageResourceViewDescription description;
    ImageFormat format;
    ResourceViewType type;
    VkImageView handle;
};

struct VulkanAccelerationStructureResourceView
{
    VkAccelerationStructureKHR handle;
};

VkImageView create_image_view(const VulkanImageResource& resource, const ImageResourceViewDescription& desc);
void free_image_view(VkImageView view);

} // namespace Mizu::Vulkan