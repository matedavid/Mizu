#include "vulkan_resource_view.h"

#include "vulkan_buffer_resource.h"
#include "vulkan_context.h"
#include "vulkan_core.h"
#include "vulkan_image_resource.h"

namespace Mizu::Vulkan
{

class VulkanImageView
{
  public:
    VulkanImageView(std::shared_ptr<ImageResource> resource, ImageFormat format, ImageResourceViewRange range)
        : m_view(VK_NULL_HANDLE)
        , m_format(format)
        , m_range(range)
        , m_resource(std::move(resource))
    {
        const VulkanImageResource& native_image = dynamic_cast<const VulkanImageResource&>(*m_resource);

        m_image_usage = native_image.get_usage();

        VkImageViewCreateInfo view_create_info{};
        view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_create_info.image = native_image.handle();
        view_create_info.viewType = get_vulkan_image_view_type(native_image.get_image_type());
        view_create_info.format = VulkanImageResource::get_vulkan_image_format(m_format);

        if (view_create_info.viewType == VK_IMAGE_VIEW_TYPE_CUBE && range.get_layer_count() != 6)
        {
            // If we create a view of a specific face, the viewType should be 2D.
            // TODO: Think better way of handling these type of conditions.
            view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        }

        if (view_create_info.viewType == VK_IMAGE_VIEW_TYPE_2D && range.get_layer_count() != 1)
        {
            // To take into account images that have multiple layers, convert them to 2D Array
            // TODO: Think better way of handling these type of conditions.
            view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        }

        if (ImageUtils::is_depth_format(m_format))
            view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        else
            view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        view_create_info.subresourceRange.baseMipLevel = m_range.get_mip_base();
        view_create_info.subresourceRange.levelCount = m_range.get_mip_count();
        view_create_info.subresourceRange.baseArrayLayer = m_range.get_layer_base();
        view_create_info.subresourceRange.layerCount = m_range.get_layer_count();

        VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &m_view));
    }

    ~VulkanImageView() { vkDestroyImageView(VulkanContext.device->handle(), m_view, nullptr); }

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

    VkImageView handle() const { return m_view; }
    ImageFormat get_format() const { return m_format; }
    ImageUsageBits get_image_usage() const { return m_image_usage; }
    ImageResourceViewRange get_range() const { return m_range; }

  private:
    VkImageView m_view;
    ImageFormat m_format;
    ImageUsageBits m_image_usage;
    ImageResourceViewRange m_range;

    std::shared_ptr<ImageResource> m_resource;
};

//
// VulkanShaderResourceView
//

VulkanShaderResourceView::VulkanShaderResourceView(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)
{
    MIZU_ASSERT(
        resource->get_usage() & ImageUsageBits::Sampled, "Can't create SRV of image without the Sampled usage bit");
    m_image_view = std::make_unique<VulkanImageView>(resource, resource->get_format(), range);
}

VulkanShaderResourceView::VulkanShaderResourceView(std::shared_ptr<BufferResource> resource)
{
    MIZU_ASSERT(
        VulkanBufferResource::get_vulkan_usage(resource->get_usage()) & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        "In Vulkan, ReadOnly buffers are declared as storage buffers, so they need the "
        "VK_BUFFER_USAGE_STORAGE_BUFFER_BIT usage bit");
    m_buffer = std::static_pointer_cast<VulkanBufferResource>(resource);
}

VulkanShaderResourceView::~VulkanShaderResourceView()
{
    // Destructor here to be able to have VulkanImageView implementation only on this cpp file
}

VkImageView VulkanShaderResourceView::get_image_view_handle() const
{
    MIZU_ASSERT(m_image_view != nullptr, "Can't get image view handle of a non texture srv");
    return m_image_view->handle();
}

const VulkanBufferResource& VulkanShaderResourceView::get_buffer() const
{
    MIZU_ASSERT(m_buffer != nullptr, "Can't get buffer of a non buffer srv");
    return *m_buffer;
}

//
// VulkanUnorderedAccessView
//

VulkanUnorderedAccessView::VulkanUnorderedAccessView(
    const std::shared_ptr<ImageResource>& resource,
    ImageResourceViewRange range)

{
    MIZU_ASSERT(
        resource->get_usage() & ImageUsageBits::UnorderedAccess,
        "Can't create UAV of image without the UnorderedAccess usage bit");
    m_image_view = std::make_unique<VulkanImageView>(resource, resource->get_format(), range);
}

VulkanUnorderedAccessView::VulkanUnorderedAccessView(std::shared_ptr<BufferResource> resource)
{
    MIZU_ASSERT(
        resource->get_usage() & BufferUsageBits::UnorderedAccess,
        "Can't create UAV of buffer without UnorderedAccess usage bit");
    m_buffer = std::static_pointer_cast<VulkanBufferResource>(resource);
}

VulkanUnorderedAccessView::~VulkanUnorderedAccessView()
{
    // Destructor here to be able to have VulkanImageView implementation only on this cpp file
}

VkImageView VulkanUnorderedAccessView::get_image_view_handle() const
{
    MIZU_ASSERT(m_image_view != nullptr, "Can't get image view handle of a non texture uav");
    return m_image_view->handle();
}

const VulkanBufferResource& VulkanUnorderedAccessView::get_buffer() const
{
    MIZU_ASSERT(m_buffer != nullptr, "Can't get buffer of a non buffer uav");
    return *m_buffer;
}

//
// VulkanConstantBufferView
//

VulkanConstantBufferView::VulkanConstantBufferView(std::shared_ptr<BufferResource> resource)
{
    m_buffer = std::static_pointer_cast<VulkanBufferResource>(resource);
    MIZU_ASSERT(
        resource->get_usage() & BufferUsageBits::ConstantBuffer,
        "Can't create CBV of buffer without ConstantBuffer usage bit");
}

const VulkanBufferResource& VulkanConstantBufferView::get_buffer() const
{
    return *m_buffer;
}

//
// VulkanRenderTargetView
//

VulkanRenderTargetView::VulkanRenderTargetView(
    std::shared_ptr<ImageResource> resource,
    ImageFormat format,
    ImageResourceViewRange range)
{
    m_image_view = std::make_unique<VulkanImageView>(resource, format, range);
    MIZU_ASSERT(
        m_image_view->get_image_usage() & ImageUsageBits::Attachment,
        "Can't create RTV with image without Attachment usage bit");
}

VulkanRenderTargetView::~VulkanRenderTargetView()
{
    // Destructor here to be able to have VulkanImageView implementation only on this cpp file
}

VkImageView VulkanRenderTargetView::handle() const
{
    return m_image_view->handle();
}

ImageFormat VulkanRenderTargetView::get_format() const
{
    return m_image_view->get_format();
}

ImageResourceViewRange VulkanRenderTargetView::get_range() const
{
    return m_image_view->get_range();
}

} // namespace Mizu::Vulkan
