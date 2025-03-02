#include "vulkan_resource_view.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu::Vulkan
{

Vulkan::VulkanImageResourceView::VulkanImageResourceView(std::shared_ptr<ImageResource> resource,
                                                         Range mip_range,
                                                         Range layer_range)
    : m_resource(std::dynamic_pointer_cast<VulkanImageResource>(std::move(resource)))
    , m_mip_range(mip_range)
    , m_layer_range(layer_range)
{
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_resource->get_image_handle();
    view_create_info.viewType = get_vulkan_image_view_type(m_resource->get_image_type());
    view_create_info.format = VulkanImageResource::get_image_format(m_resource->get_format());

    if (ImageUtils::is_depth_format(m_resource->get_format()))
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

ImageFormat VulkanImageResourceView::get_format() const
{
    return m_resource->get_format();
}

VulkanImageResourceView::Range VulkanImageResourceView::get_mip_range() const
{
    return m_mip_range;
}

VulkanImageResourceView::Range VulkanImageResourceView::get_layer_range() const
{
    return m_layer_range;
}

ImageUsageBits VulkanImageResourceView::get_image_usage() const
{
    return m_resource->get_usage();
}

VkImageViewType VulkanImageResourceView::get_vulkan_image_view_type(ImageType type)
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

} // namespace Mizu::Vulkan
