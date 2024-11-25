#include "vulkan_image_resource.h"

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_device_memory_allocator.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanImageResource::VulkanImageResource(const ImageDescription& desc,
                                         const SamplingOptions& sampling,
                                         std::weak_ptr<IDeviceMemoryAllocator> allocator)
      : m_description(desc), m_sampling_options(sampling), m_allocator(allocator) {
    create_image();

    if (std::shared_ptr<IDeviceMemoryAllocator> tmp_allocator = m_allocator.lock()) {
        m_allocation = tmp_allocator->allocate_image_resource(*this);
    } else {
        MIZU_UNREACHABLE("Couldn't allocate image resource");
    }

    create_image_views();
    create_sampler();
}

VulkanImageResource::VulkanImageResource(const ImageDescription& desc,
                                         const SamplingOptions& sampling,
                                         const std::vector<uint8_t>& content,
                                         std::weak_ptr<IDeviceMemoryAllocator> allocator)
      : VulkanImageResource(desc, sampling, allocator) {
    // Transition image layout for copying
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(*this, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        });

    // Create staging buffer
    const auto staging_buffer = VulkanBuffer{
        content.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    };
    staging_buffer.copy_data(content.data());

    // Copy staging buffer to image
    staging_buffer.copy_to_image(*this);

    // Transition image layout for shader access
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(
                *this, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
        });
}

VulkanImageResource::VulkanImageResource(uint32_t width,
                                         uint32_t height,
                                         VkImage image,
                                         VkImageView image_view,
                                         bool owns_resources)
      : m_description({}), m_allocator() {
    m_description.width = width;
    m_description.height = height;

    m_owns_resources = owns_resources;

    m_image = image;
    m_image_view = image_view;
}

VulkanImageResource::~VulkanImageResource() {
    if (std::shared_ptr<IDeviceMemoryAllocator> allocator = m_allocator.lock()) {
        allocator->release(m_allocation);
    }

    if (m_owns_resources) {
        vkDestroySampler(VulkanContext.device->handle(), m_sampler, nullptr);
        vkDestroyImageView(VulkanContext.device->handle(), m_image_view, nullptr);
        vkDestroyImage(VulkanContext.device->handle(), m_image, nullptr);
    }
}

void VulkanImageResource::create_image() {
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

    // m_current_state = ImageResourceState::Undefined;

    VK_CHECK(vkCreateImage(VulkanContext.device->handle(), &image_create_info, nullptr, &m_image));
}

void VulkanImageResource::create_image_views() {
    VkImageViewCreateInfo view_create_info{};
    view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_create_info.image = m_image;
    view_create_info.viewType = get_image_view_type(m_description.type);
    view_create_info.format = get_image_format(m_description.format);

    if (ImageUtils::is_depth_format(m_description.format))
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    else
        view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    view_create_info.subresourceRange.baseMipLevel = 0;
    view_create_info.subresourceRange.levelCount = m_description.num_mips;
    view_create_info.subresourceRange.baseArrayLayer = 0;
    view_create_info.subresourceRange.layerCount = m_description.num_layers;

    // TODO: Create mip and layer image views

    VK_CHECK(vkCreateImageView(VulkanContext.device->handle(), &view_create_info, nullptr, &m_image_view));
}

void VulkanImageResource::create_sampler() {
    // TODO: Still a lot of parameters need to be configured
    VkSamplerCreateInfo sampler_create_info{};
    sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_create_info.magFilter = get_vulkan_filter(m_sampling_options.magnification_filter);
    sampler_create_info.minFilter = get_vulkan_filter(m_sampling_options.minification_filter);
    sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_create_info.addressModeU = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_u);
    sampler_create_info.addressModeV = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_v);
    sampler_create_info.addressModeW = get_vulkan_sampler_address_mode(m_sampling_options.address_mode_w);
    sampler_create_info.mipLodBias = 0.0f;
    sampler_create_info.anisotropyEnable = VK_FALSE;
    sampler_create_info.maxAnisotropy = 0.0f;
    sampler_create_info.compareEnable = VK_FALSE;
    sampler_create_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_create_info.minLod = 0.0f;
    sampler_create_info.maxLod = 0.0f;
    sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    sampler_create_info.unnormalizedCoordinates = VK_FALSE;

    VK_CHECK(vkCreateSampler(VulkanContext.device->handle(), &sampler_create_info, nullptr, &m_sampler));
}

VkImageType VulkanImageResource::get_image_type(ImageType type) {
    switch (type) {
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

VkFormat VulkanImageResource::get_image_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::RGBA8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::RGBA8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::RGBA16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::BGRA8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    }
}

VkImageViewType VulkanImageResource::get_image_view_type(ImageType type) {
    switch (type) {
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

VkImageLayout VulkanImageResource::get_vulkan_image_resource_state(ImageResourceState state) {
    switch (state) {
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
    }
}

VkFilter VulkanImageResource::get_vulkan_filter(ImageFilter filter) {
    switch (filter) {
    case ImageFilter::Nearest:
        return VK_FILTER_NEAREST;
    case ImageFilter::Linear:
        return VK_FILTER_LINEAR;
    }
}

VkSamplerAddressMode VulkanImageResource::get_vulkan_sampler_address_mode(ImageAddressMode mode) {
    switch (mode) {
    case ImageAddressMode::Repeat:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    case ImageAddressMode::MirroredRepeat:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case ImageAddressMode::ClampToEdge:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    case ImageAddressMode::ClampToBorder:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    }
}

uint32_t VulkanImageResource::get_vulkan_usage() const {
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

VkImageFormatProperties VulkanImageResource::get_format_properties() const {
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