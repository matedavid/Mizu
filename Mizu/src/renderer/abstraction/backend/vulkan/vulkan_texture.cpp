#include "vulkan_texture.h"

#include <stb_image.h>

#include "renderer/abstraction/backend/vulkan/vk_core.h"
#include "renderer/abstraction/backend/vulkan/vulkan_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanTexture2D::VulkanTexture2D(const ImageDescription& desc) {
    VulkanImage::Description description{};
    description.width = desc.width;
    description.height = desc.height;
    description.depth = 1;
    description.format = desc.format;
    description.type = VK_IMAGE_TYPE_2D;
    description.usage = desc.usage;
    description.flags = 0;
    description.num_mips = desc.generate_mips ? ImageUtils::compute_num_mips(desc.width, desc.height) : 1;
    description.num_layers = 1;

    init_resources(description, desc.sampling_options);
}

VulkanTexture2D::VulkanTexture2D(const std::filesystem::path& path, SamplingOptions sampling) {
    const auto& str_path = path.string();
    MIZU_ASSERT(std::filesystem::exists(path), "Texture with path {} does not exist", str_path);

    int32_t width, height, channels;
    const stbi_uc* pixels = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    MIZU_ASSERT(pixels != nullptr, "Could not open texture with path {}", str_path);

    const uint32_t size = static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4;

    VulkanImage::Description description{};
    description.width = static_cast<uint32_t>(width);
    description.height = static_cast<uint32_t>(height);
    description.depth = 1;
    description.format = ImageFormat::RGBA8_SRGB;
    description.type = VK_IMAGE_TYPE_2D;
    description.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    description.flags = 0;
    description.num_mips = 1; // TODO: desc.generate_mips ? ImageUtils::compute_num_mips(desc.width, desc.height) : 1;
    description.num_layers = 1;

    init_resources(description, sampling);

    // TODO: Really dont like to separate it in three command buffer submissions, look into if it can be done in a
    // single command buffer

    // Transition image layout for copying
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(*this, ImageResourceState::Undefined, ImageResourceState::TransferDst);
        });

    VulkanTransferCommandBuffer::submit_single_time([&](const VulkanTransferCommandBuffer& command_buffer) {
        // Create staging buffer
        const auto staging_buffer = VulkanBuffer{
            size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        staging_buffer.copy_data(pixels);

        // Copy staging buffer to image
        staging_buffer.copy_to_image(*this);
    });

    // Transition image layout for shader access
    VulkanRenderCommandBuffer::submit_single_time(
        [&](const VulkanCommandBufferBase<CommandBufferType::Graphics>& command_buffer) {
            command_buffer.transition_resource(
                *this, ImageResourceState::TransferDst, ImageResourceState::ShaderReadOnly);
        });
}

VulkanTexture2D::VulkanTexture2D(uint32_t width, uint32_t height, VkImage image, VkImageView view, bool owning)
      : VulkanImage(image, view, owning) {
    m_description.width = width;
    m_description.height = height;
}

} // namespace Mizu::Vulkan
