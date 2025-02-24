#include "vulkan_resource_view.h"

#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu::Vulkan
{

Vulkan::VulkanImageResourceView::VulkanImageResourceView(const ImageResource& resource,
                                                         Range mip_range,
                                                         Range layer_range)
{
    const VulkanImageResource& native_resource = dynamic_cast<const VulkanImageResource&>(resource);

    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = native_resource.get_image_handle();
    view_create_info.viewType = VulkanImageResource::get_image_view_type(native_resource.get_image_type());
    view_create_info.format = VulkanImageResource::get_image_format(native_resource.get_format());

    if (ImageUtils::is_depth_format(native_resource.get_format()))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = mip_range.get_base();
    view_create_info.subresourceRange.levelCount = mip_range.get_count();
    view_create_info.subresourceRange.baseArrayLayer = layer_range.get_base();
    view_create_info.subresourceRange.layerCount = layer_range.get_count();

    VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &m_view));
}

VulkanImageResourceView::~VulkanImageResourceView()
{
    vkDestroyImageView(VulkanContext.device->handle(), m_view, nullptr);
}

} // namespace Mizu::Vulkan
