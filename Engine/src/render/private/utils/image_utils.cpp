#include "render/utils/image_utils.h"

#include "render/runtime/renderer.h"
#include "render/utils/buffer_utils.h"

namespace Mizu
{

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

} // namespace Mizu
