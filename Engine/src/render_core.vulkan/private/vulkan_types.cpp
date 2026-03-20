#include "vulkan_types.h"

#include "vulkan_context.h"

namespace Mizu::Vulkan
{

VkBufferUsageFlags get_vulkan_buffer_usage(BufferUsageBits usage)
{
    VkBufferUsageFlags vulkan_usage = 0;

    if (usage & BufferUsageBits::VertexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (usage & BufferUsageBits::IndexBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (usage & BufferUsageBits::ConstantBuffer)
        vulkan_usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (usage & BufferUsageBits::ShaderResource)
        vulkan_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & BufferUsageBits::UnorderedAccess)
        vulkan_usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (usage & BufferUsageBits::TransferSrc)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (usage & BufferUsageBits::TransferDst)
        vulkan_usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (usage & BufferUsageBits::RtxAccelerationStructureStorage)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    if (usage & BufferUsageBits::RtxAccelerationStructureInputReadOnly)
        vulkan_usage |=
            (VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
             | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    if (usage & BufferUsageBits::RtxShaderBindingTable)
        vulkan_usage |= (VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);

    return vulkan_usage;
}

VkImageType get_vulkan_image_type(ImageType type)
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

VkFormat get_vulkan_image_format(ImageFormat format)
{
    switch (format)
    {
    case ImageFormat::R32_SFLOAT:
        return VK_FORMAT_R32_SFLOAT;
    case ImageFormat::R16G16_SFLOAT:
        return VK_FORMAT_R16G16_SFLOAT;
    case ImageFormat::R32G32_SFLOAT:
        return VK_FORMAT_R32G32_SFLOAT;
    case ImageFormat::R32G32B32_SFLOAT:
        return VK_FORMAT_R32G32B32_SFLOAT;
    case ImageFormat::R8G8B8A8_SRGB:
        return VK_FORMAT_R8G8B8A8_SRGB;
    case ImageFormat::R8G8B8A8_UNORM:
        return VK_FORMAT_R8G8B8A8_UNORM;
    case ImageFormat::R16G16B16A16_SFLOAT:
        return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ImageFormat::R32G32B32A32_SFLOAT:
        return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ImageFormat::B8G8R8A8_SRGB:
        return VK_FORMAT_B8G8R8A8_SRGB;
    case ImageFormat::B8G8R8A8_UNORM:
        return VK_FORMAT_B8G8R8A8_UNORM;
    case ImageFormat::D32_SFLOAT:
        return VK_FORMAT_D32_SFLOAT;
    }
}

VkImageLayout get_vulkan_image_resource_state(ImageResourceState state)
{
    switch (state)
    {
    case ImageResourceState::Undefined:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case ImageResourceState::ShaderReadOnly:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case ImageResourceState::UnorderedAccess:
        return VK_IMAGE_LAYOUT_GENERAL;
    case ImageResourceState::TransferSrc:
        return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    case ImageResourceState::TransferDst:
        return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    case ImageResourceState::ColorAttachment:
        return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    case ImageResourceState::DepthStencilAttachment:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    case ImageResourceState::Present:
        return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    }
}

VkImageUsageFlags get_vulkan_image_usage(ImageUsageBits usage, ImageFormat format)
{
    VkImageUsageFlags vulkan_usage = 0;

    const bool has_usage_attachment = usage & ImageUsageBits::Attachment;
    if (has_usage_attachment && is_depth_format(format))
        vulkan_usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    else if (has_usage_attachment)
        vulkan_usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (usage & ImageUsageBits::Sampled)
        vulkan_usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    if (usage & ImageUsageBits::UnorderedAccess)
        vulkan_usage |= VK_IMAGE_USAGE_STORAGE_BIT;

    if (usage & ImageUsageBits::TransferSrc)
        vulkan_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (usage & ImageUsageBits::TransferDst)
        vulkan_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    return vulkan_usage;
}

VkSharingMode get_vulkan_sharing_mode(ResourceSharingMode mode)
{
    switch (mode)
    {
    case ResourceSharingMode::Exclusive:
        return VK_SHARING_MODE_EXCLUSIVE;
    case ResourceSharingMode::Concurrent:
        return VK_SHARING_MODE_CONCURRENT;
    }
}

void get_vulkan_queue_families_array(typed_bitset<CommandBufferType> bitset, QueueFamiliesArray& out_queue_families)
{
    static constexpr std::array queue_families = {
        CommandBufferType::Graphics, CommandBufferType::Compute, CommandBufferType::Transfer};

    for (CommandBufferType type : queue_families)
    {
        if (bitset.test(type) && VulkanContext.device->is_queue_available(type))
        {
            out_queue_families.push_back(VulkanContext.device->get_queue(type)->family());
        }
        else if (bitset.test(type) && !VulkanContext.device->is_queue_available(type))
        {
            MIZU_LOG_WARNING("Trying to use a queue family that is not available, ignoring");
        }
    }
}

VkAttachmentLoadOp get_vulkan_load_operation(LoadOperation op)
{
    switch (op)
    {
    case LoadOperation::Load:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    case LoadOperation::Clear:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::DontCare:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

VkAttachmentStoreOp get_vulkan_store_operation(StoreOperation op)
{
    switch (op)
    {
    case StoreOperation::Store:
        return VK_ATTACHMENT_STORE_OP_STORE;
    case StoreOperation::DontCare:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::None:
        return VK_ATTACHMENT_STORE_OP_NONE;
    }
}

} // namespace Mizu::Vulkan
