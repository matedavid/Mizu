#include "vulkan_cubemap.h"

#include <stb_image.h>

#include "vk_core.h"

#include "renderer/abstraction/backend/vulkan/vulkan_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_command_buffer.h"
#include "renderer/abstraction/backend/vulkan/vulkan_context.h"
#include "renderer/abstraction/backend/vulkan/vulkan_image.h"

#include "utility/assert.h"

namespace Mizu::Vulkan {

VulkanCubemap::VulkanCubemap(const ImageDescription& desc) {
    // TODO:
    MIZU_UNREACHABLE("Unimplemented");
}

VulkanCubemap::VulkanCubemap(const Faces& faces) {
    std::vector<uint8_t> data;
    uint32_t width, height;

    // Order from:
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/html/chap16.html#_cube_map_face_selection
    load_face(faces.right, data, width, height, 0);
    load_face(faces.left, data, width, height, 1);
    load_face(faces.top, data, width, height, 2);
    load_face(faces.bottom, data, width, height, 3);
    load_face(faces.front, data, width, height, 4);
    load_face(faces.back, data, width, height, 5);

    VulkanImage::Description description{};
    description.width = width;
    description.height = height;
    description.depth = 1;
    description.format = ImageFormat::RGBA8_SRGB;
    description.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    description.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    description.num_layers = 6;

    const uint32_t expected_size = width * height * 4 * 6;
    MIZU_ASSERT(
        data.size() == expected_size, "Data size and expected size do not match {} != {}", data.size(), expected_size);

    init_resources(description, SamplingOptions{});

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
            data.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };
        staging_buffer.copy_data(data.data());

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

void VulkanCubemap::load_face(const std::filesystem::path& path,
                              std::vector<uint8_t>& data,
                              uint32_t& width,
                              uint32_t& height,
                              uint32_t idx) {
    MIZU_ASSERT(std::filesystem::exists(path), "Cubemap face: {} does not exist", path.string());

    const std::string str_path = path.string();

    int32_t width_local, height_local, channels_local;
    uint8_t* face_data = stbi_load(str_path.c_str(), &width_local, &height_local, &channels_local, STBI_rgb_alpha);
    MIZU_ASSERT(face_data, "Failed to load cubemap face: {}", path.string());

    const uint32_t size = width_local * height_local * 4;
    if (idx == 0) {
        width = static_cast<uint32_t>(width_local);
        height = static_cast<uint32_t>(height_local);

        data = std::vector<uint8_t>(size * 6);
    } else {
        MIZU_ASSERT(width == static_cast<uint32_t>(width_local) && height == static_cast<uint32_t>(height_local),
                    "Width and Height values of face {} do not match with previous faces",
                    str_path);
    }

    std::memcpy(data.data() + idx * size, face_data, size);

    stbi_image_free(face_data);
}

} // namespace Mizu::Vulkan