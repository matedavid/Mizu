#include "vulkan_resource_view.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_image_resource.h"

namespace Mizu::Vulkan
{

Vulkan::VulkanImageResourceView::VulkanImageResourceView(
    std::shared_ptr<ImageResource> resource,
    ImageResourceViewRange range)
    : m_resource(std::dynamic_pointer_cast<VulkanImageResource>(std::move(resource)))
    , m_range(range)
{
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_resource->get_image_handle();
    view_create_info.viewType = get_vulkan_image_view_type(m_resource->get_image_type());
    view_create_info.format = VulkanImageResource::get_image_format(m_resource->get_format());

    if (view_create_info.viewType == VK_IMAGE_VIEW_TYPE_CUBE && range.get_layer_count() != 6)
    {
        // If we create a view of a specific face, the viewType should be 2D.
        // TODO: Think better way of handling these type of conditions.
        view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    if (ImageUtils::is_depth_format(m_resource->get_format()))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = m_range.get_mip_base();
    view_create_info.subresourceRange.levelCount = m_range.get_mip_count();
    view_create_info.subresourceRange.baseArrayLayer = m_range.get_layer_base();
    view_create_info.subresourceRange.layerCount = m_range.get_layer_count();

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
