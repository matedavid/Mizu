#include "cubemap.h"

#include <cstring>
#include <filesystem>
#include <stb_image.h>

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu
{

std::shared_ptr<Cubemap> Cubemap::create(const Cubemap::Faces& faces,
                                         const SamplingOptions& sampling,
                                         std::weak_ptr<IDeviceMemoryAllocator> allocator)
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

            data = std::vector<uint8_t>(width * height * 4 * 6);
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

    const uint32_t expected_size = width * height * 4 * 6;
    MIZU_ASSERT(content.size() == expected_size,
                "Cubemap data size does not match expected size ({} != {})",
                expected_size,
                content.size());

    ImageDescription desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.type = ImageType::Cubemap;
    desc.format = ImageFormat::RGBA8_SRGB; // TODO: Make configurable...
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1; // TODO: Make configurable...
    desc.num_layers = 6;

    return std::make_shared<Cubemap>(ImageResource::create(desc, sampling, content, allocator));
}

std::shared_ptr<Cubemap> Cubemap::create(const Cubemap::Description& desc,
                                         const SamplingOptions& sampling,
                                         std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    return std::make_shared<Cubemap>(ImageResource::create(Cubemap::get_image_description(desc), sampling, allocator));
}

ImageDescription Cubemap::get_image_description(const Description& desc)
{
    ImageDescription image_desc{};
    image_desc.width = desc.dimensions.x;
    image_desc.height = desc.dimensions.y;
    image_desc.depth = 1;
    image_desc.format = desc.format;
    image_desc.usage = desc.usage;
    image_desc.num_mips =
        desc.generate_mips ? ImageUtils::compute_num_mips(image_desc.width, image_desc.height, image_desc.depth) : 1;
    image_desc.num_layers = 6;

    return image_desc;
}

} // namespace Mizu