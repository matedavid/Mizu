#include "renderer/core/image_utils.h"

#include <stb_image.h>

#include "base/debug/assert.h"

namespace Mizu
{

std::shared_ptr<ImageResource> ImageUtils::create_texture2d(const std::filesystem::path& path)
{
    const std::string& str_path = path.string();
    MIZU_ASSERT(std::filesystem::exists(path), "Could not find path: {}", path.string());

    int32_t width, height, channels;
    uint8_t* content_raw = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    MIZU_ASSERT(content_raw != nullptr, "Could not load image file: {}", str_path);

    ImageDescription desc{};
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.depth = 1;
    desc.type = ImageType::Image2D;
    desc.format = ImageFormat::R8G8B8A8_SRGB; // TODO: Make configurable...
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1; // TODO: Should make this configurable???
    desc.num_layers = 1;

    const auto resource = g_render_device->create_image(desc);
    BufferUtils::initialize_image(*resource, content_raw);

    stbi_image_free(content_raw);

    return resource;
}

std::shared_ptr<ImageResource> ImageUtils::create_texture2d(
    glm::uvec2 dimensions,
    ImageFormat format,
    std::span<uint8_t> content,
    std::string name)
{
    ImageDescription desc{};
    desc.width = dimensions.x;
    desc.height = dimensions.y;
    desc.depth = 1;
    desc.type = ImageType::Image2D;
    desc.format = format;
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1;
    desc.num_layers = 1;
    desc.name = std::move(name);

    return create_texture2d(desc, content);
}

std::shared_ptr<ImageResource> ImageUtils::create_texture2d(const ImageDescription& desc, std::span<uint8_t> content)
{
    const auto resource = g_render_device->create_image(desc);
    BufferUtils::initialize_image(*resource, content.data());

    return resource;
}

std::shared_ptr<ImageResource> ImageUtils::create_cubemap(const Faces& faces, std::string name)
{
    uint32_t width = 0, height = 0;
    const auto load_face = [&](const std::filesystem::path& path, uint32_t idx, std::vector<uint8_t>& data) {
        const std::string& str_path = path.string();

        int32_t width_local, height_local, channels_local;
        uint8_t* content = stbi_load(str_path.c_str(), &width_local, &height_local, &channels_local, STBI_rgb_alpha);

        if (idx == 0)
        {
            width = static_cast<uint32_t>(width_local);
            height = static_cast<uint32_t>(height_local);

            data = std::vector<uint8_t>(width * height * 6 * 4 * sizeof(uint8_t));
        }
        else if (static_cast<uint32_t>(width_local) != width || static_cast<uint32_t>(height_local) != height)
        {
            MIZU_LOG_WARNING(
                "Cubemap face dimensions do not match with the rest of the faces (width: {} != {}, height: {} != {})",
                width_local,
                width,
                height_local,
                height);
        }

        const uint32_t size = width * height * 4;
        memcpy(data.data() + idx * size, content, size);

        stbi_image_free(content);
    };

    // Order: +X -X +Y -Y +Z -Z
    std::vector<uint8_t> content;
    load_face(faces.right, 0, content);
    load_face(faces.left, 1, content);
    load_face(faces.top, 2, content);
    load_face(faces.bottom, 3, content);
    load_face(faces.front, 4, content);
    load_face(faces.back, 5, content);

    [[maybe_unused]] const uint32_t expected_size = width * height * 6 * 4 * sizeof(uint8_t);
    MIZU_ASSERT(
        content.size() == expected_size,
        "Cubemap data size does not match expected size ({} != {})",
        expected_size,
        content.size());

    ImageDescription desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.type = ImageType::Cubemap;
    desc.format = ImageFormat::R8G8B8A8_SRGB; // TODO: Make configurable...
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1; // TODO: Should make this configurable???
    desc.num_layers = 6;
    desc.name = std::move(name);

    const auto resource = g_render_device->create_image(desc);
    BufferUtils::initialize_image(*resource, content.data());

    return resource;
}

} // namespace Mizu
