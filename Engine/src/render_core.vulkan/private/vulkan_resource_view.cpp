#include "vulkan_resource_view.h"

#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_image_resource.h"

namespace Mizu::Vulkan
{

static VkImageViewType get_vulkan_image_view_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return VK_IMAGE_VIEW_TYPE_1D;
    case ImageType::Image2D:
        return VK_IMAGE_VIEW_TYPE_2D;
    case ImageType::Image3D:
        return VK_IMAGE_VIEW_TYPE_3D;
    case ImageType::Cubemap:
        return VK_IMAGE_VIEW_TYPE_CUBE;
    }
}

VkImageView create_image_view(const ImageResourceViewDescription desc, const VulkanImageResource& resource)
{
    const ImageFormat format = desc.override_format.value_or(resource.get_format());

    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = resource.handle();
    view_create_info.viewType = get_vulkan_image_view_type(resource.get_image_type());
    view_create_info.format = VulkanImageResource::get_vulkan_image_format(format);

    if (view_create_info.viewType == VK_IMAGE_VIEW_TYPE_CUBE && desc.layer_count != 6)
    {
        // If we create a view of a specific face, the viewType should be 2D.
        // TODO: Think better way of handling these type of conditions.
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    if (view_create_info.viewType == VK_IMAGE_VIEW_TYPE_2D && desc.layer_count != 1)
    {
        // To take into account images that have multiple layers, convert them to 2D Array
        // TODO: Think better way of handling these type of conditions.
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    if (is_depth_format(format))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = desc.mip_base;
    view_create_info.subresourceRange.levelCount = desc.mip_count;
    view_create_info.subresourceRange.baseArrayLayer = desc.layer_base;
    view_create_info.subresourceRange.layerCount = desc.layer_count;

    VkImageView view;
    VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &view));

    return view;
}

void free_image_view(VkImageView view)
{
    vkDestroyImageView(VulkanContext.device->handle(), view, nullptr);
}

VulkanBufferResourceView* get_internal_buffer_resource_view(const ResourceView& view)
{
    VulkanBufferResourceView* internal_view = reinterpret_cast<VulkanBufferResourceView*>(view.internal);
    MIZU_ASSERT(internal_view != nullptr, "Failed to get internal buffer resource view");

    return internal_view;
}

VulkanImageResourceView* get_internal_image_resource_view(const ResourceView& view)
{
    VulkanImageResourceView* internal_view = reinterpret_cast<VulkanImageResourceView*>(view.internal);
    MIZU_ASSERT(internal_view != nullptr, "Failed to get internal image resource view");

    return internal_view;
}

VulkanAccelerationStructureResourceView* get_internal_acceleration_structure_resource_view(const ResourceView& view)
{
    VulkanAccelerationStructureResourceView* internal_view =
        reinterpret_cast<VulkanAccelerationStructureResourceView*>(view.internal);
    MIZU_ASSERT(internal_view != nullptr, "Failed to get internal acceleration structure resource view");

    return internal_view;
}

} // namespace Mizu::Vulkan
