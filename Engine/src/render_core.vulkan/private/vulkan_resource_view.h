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

struct VulkanAccelerationStructureResourceView
{
    VkAccelerationStructureKHR handle;
};

VkImageView create_image_view(const ImageResourceViewDescription desc, const VulkanImageResource& resource);
void free_image_view(VkImageView view);

VulkanBufferResourceView* get_internal_buffer_resource_view(const ResourceView& view);
VulkanImageResourceView* get_internal_image_resource_view(const ResourceView& view);
VulkanAccelerationStructureResourceView* get_internal_acceleration_structure_resource_view(const ResourceView& view);

} // namespace Mizu::Vulkan