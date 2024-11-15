#include "device_memory_allocator.h"

#include <stb_image.h>

#include "renderer/abstraction/renderer.h"

#include "renderer/abstraction/backend/opengl/opengl_device_memory_allocator.h"
#include "renderer/abstraction/backend/vulkan/vulkan_device_memory_allocator.h"

#include "utility/assert.h"

namespace Mizu {

//
// IDeviceMemoryAllocator
//

std::shared_ptr<Texture2D> IDeviceMemoryAllocator::allocate_texture(const std::filesystem::path& path,
                                                                    const SamplingOptions& sampling) {
    const std::string& str_path = path.string();

    MIZU_ASSERT(std::filesystem::exists(path), "Could not find path: {}", path.string());

    int32_t width, height, channels;
    uint8_t* content = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    MIZU_ASSERT(content != nullptr, "Could not load image file: {}", str_path);

    ImageDescription desc{};
    desc.width = static_cast<uint32_t>(width);
    desc.height = static_cast<uint32_t>(height);
    desc.depth = 1;
    desc.type = ImageType::Image2D;
    desc.format = ImageFormat::RGBA8_SRGB; // TODO: Make configurable...
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1; // TODO: Make configurable...
    desc.num_layers = 1;

    const uint32_t size = desc.width * desc.height * 4;
    const auto resource = allocate_image_resource(desc, sampling, content, size);

    stbi_image_free(content);

    return std::make_shared<Texture2D>(resource);
}

std::shared_ptr<Cubemap> IDeviceMemoryAllocator::allocate_cubemap(const Cubemap::Faces& faces,
                                                                  const SamplingOptions& sampling) {
    uint32_t width = 0, height = 0;
    const auto load_face = [&](const std::filesystem::path& path, uint32_t idx, std::vector<uint8_t>& data) {
        const std::string& str_path = path.string();

        int32_t width_local, height_local, channels_local;
        uint8_t* content = stbi_load(str_path.c_str(), &width_local, &height_local, &channels_local, STBI_rgb_alpha);

        if (idx == 0) {
            width = static_cast<uint32_t>(width_local);
            height = static_cast<uint32_t>(height_local);

            data = std::vector<uint8_t>(width * height * 4 * 6);
        } else if (static_cast<uint32_t>(width_local) != width || static_cast<uint32_t>(height_local) != height) {
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

    return std::make_shared<Cubemap>(allocate_image_resource(desc, sampling, content.data(), expected_size));
}

std::shared_ptr<Cubemap> IDeviceMemoryAllocator::allocate_cubemap(const Cubemap::Description& desc,
                                                                  const SamplingOptions& sampling) {
    return std::make_shared<Cubemap>(allocate_image_resource(Cubemap::get_image_description(desc), sampling));
}

void IDeviceMemoryAllocator::release(const std::shared_ptr<ITextureBase>& texture) {
    release(texture->get_resource());
}

void IDeviceMemoryAllocator::release(const std::shared_ptr<Cubemap>& cubemap) {
    release(cubemap->get_resource());
}

//
// BaseDeviceMemoryAllocator
//

std::shared_ptr<BaseDeviceMemoryAllocator> BaseDeviceMemoryAllocator::create() {
    switch (Renderer::get_config().graphics_api) {
    case GraphicsAPI::Vulkan:
        return std::make_shared<Vulkan::VulkanBaseDeviceMemoryAllocator>();
    case GraphicsAPI::OpenGL:
        return std::make_shared<OpenGL::OpenGLBaseDeviceMemoryAllocator>();
    }
}

} // namespace Mizu