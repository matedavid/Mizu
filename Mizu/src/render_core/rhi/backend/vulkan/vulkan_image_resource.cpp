#include "vulkan_image_resource.h"

#include "render_core/rhi/backend/vulkan/vk_core.h"
#include "render_core/rhi/backend/vulkan/vulkan_buffer_resource.h"
#include "render_core/rhi/backend/vulkan/vulkan_command_buffer.h"
#include "render_core/rhi/backend/vulkan/vulkan_context.h"
#include "render_core/rhi/backend/vulkan/vulkan_device_memory_allocator.h"

#include "utility/assert.h"

namespace Mizu::Vulkan
{

VulkanImageResource::VulkanImageResource(const ImageDescription& desc, bool aliasing)
    : m_description(desc)
    , m_aliased(aliasing)
{
}

VulkanImageResource::VulkanImageResource(const ImageDescription& desc, std::weak_ptr<IDeviceMemoryAllocator> allocator)
    : m_description(desc)
    , m_allocator(allocator)
{
    create_image();

    if (std::shared_ptr<IDeviceMemoryAllocator> tmp_allocator = m_allocator.lock())
    {
        m_allocation = tmp_allocator->allocate_image_resource(*this);
    }
    else
    {
        MIZU_UNREACHABLE("Failed to allocate image resource");
    }
}

VulkanImageResource::VulkanImageResource(const ImageDescription& desc,
                                         const uint8_t* content,
                                         std::weak_ptr<IDeviceMemoryAllocator> allocator)
    : VulkanImageResource(desc, allocator)
{
    BufferDescription staging_desc{};
    staging_desc.size = m_description.width * m_description.height * m_description.depth * m_description.num_layers
                        * ImageUtils::get_format_size(m_description.format);
    staging_desc.type = BufferType::Staging;

    VulkanBufferResource staging_buffer(staging_desc, Renderer::get_allocator());
    staging_buffer.set_data(content);

    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command) {
            command.transition_resource(*this, ImageResourceState::Undefined, ImageResourceState::TransferDst);

            command.copy_buffer_to_image(staging_buffer, *this);

            command.transition_resource(*this, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
        });
}

VulkanImageResource::VulkanImageResource(uint32_t width,
                                         uint32_t height,
                                         ImageFormat format,
                                         VkImage image,
                                         bool owns_resources)
    : m_description({})
    , m_allocator()
{
    m_description.width = width;
    m_description.height = height;
    m_description.format = format;

    m_owns_resources = owns_resources;

    m_image = image;
}

VulkanImageResource::~VulkanImageResource()
{
    if (std::shared_ptr<IDeviceMemoryAllocator> allocator = m_allocator.lock())
    {
        allocator->release(m_allocation);
    }
    else if (m_allocation != Allocation::invalid())
    {
        MIZU_UNREACHABLE("Failed to release image resource allocation");
    }

    if (m_owns_resources)
    {
        vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
        vkDestroyImage(VulkanContext.device->handle(), m_image, nullptr);
    }
}

void VulkanImageResource::create_image()
{
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = get_image_type(m_description.type);
    image_create_info.format = get_image_format(m_description.format);
    image_create_info.extent =
        VkExtent3D{.width = m_description.width, .height = m_description.height, .depth = m_description.depth};
    image_create_info.mipLevels = m_description.num_mips;
    image_create_info.arrayLayers = m_description.num_layers;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.queueFamilyIndexCount = 0;
    image_create_info.pQueueFamilyIndices = nullptr;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.flags = m_description.type == ImageType::Cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;
    image_create_info.usage = get_vulkan_usage();

    if (m_aliased)
    {
        image_create_info.flags |= VK_IMAGE_CREATE_ALIAS_BIT;
    }

    // m_current_state = ImageResourceState::Undefined;

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_image));

    if (!m_description.name.empty())
    {
        VK_DEBUG_SET_OBJECT_NAME(m_image, m_description.name);
    }
}

VkImageType VulkanImageResource::get_image_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return VK_IMAGE_TYPE_1D;
    case ImageType::Image2D:
        return VK_IMAGE_TYPE_2D;
    case ImageType::Image3D:
        return VK_IMAGE_TYPE_3D;
    case ImageType::Cubemap:
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkImageCreateInfo-flags-00949
        return VK_IMAGE_TYPE_2D;
    }
}

VkFormat VulkanImageResource::get_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case ImageFormat::RG16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormat::RG32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormat::RGB32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case ImageFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::RGBA32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    }
}

VkImageLayout VulkanImageResource::get_vulkan_image_resource_state(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageResourceState::General:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ImageResourceState::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ImageResourceState::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageResourceState::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageResourceState::DepthStencilAttachment:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageResourceState::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}

uint32_t VulkanImageResource::get_vulkan_usage() const
{
    uint32_t usage = 0;

    const bool has_usage_attachment = m_description.usage & ImageUsageBits::Attachment;
    if (has_usage_attachment && ImageUtils::is_depth_format(m_description.format))
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if (has_usage_attachment)
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (m_description.usage & ImageUsageBits::Sampled)
        usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (m_description.usage & ImageUsageBits::Storage)
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (m_description.usage & ImageUsageBits::TransferDst)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return usage;
}

VkImageFormatProperties VulkanImageResource::get_format_properties() const
{
    const uint32_t usage = get_vulkan_usage();
    const uint32_t flags = m_description.type == ImageType::Cubemap ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0;

    VkImageFormatProperties properties;
    vkGetPhysicalDeviceImageFormatProperties(VulkanContext.device->physical_device(),
                                             get_image_format(m_description.format),
                                             get_image_type(m_description.type),
                                             VK_IMAGE_TILING_OPTIMAL,
                                             usage,
                                             flags,
                                             &properties);

    return properties;
}

} // namespace Mizu::Vulkan