#include "texture.h"

#include <cstring>
#include <stb_image.h>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

#include "render_core/rhi/buffer_resource.h"

namespace Mizu
{

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const Description& desc)
{
    static_assert(std::is_base_of<ITextureBase, T>());

    const ImageDescription image_desc = TextureBase<T, DimensionsT>::get_image_description(desc);
    const std::shared_ptr<ImageResource> resource = ImageResource::create(image_desc);
    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const std::filesystem::path& path)
{
    static_assert(std::is_base_of<ITextureBase, T>());

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
    desc.format = ImageFormat::RGBA8_SRGB; // TODO: Make configurable...
    desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;
    desc.num_mips = 1; // TODO: Should make this configurable???
    desc.num_layers = 1;

    const uint32_t size = desc.width * desc.height * 4;

    const std::shared_ptr<ImageResource> resource = ImageResource::create(desc);
    BufferUtils::initialize_image(*resource, content_raw, size);

    stbi_image_free(content_raw);

    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const Description& desc, std::span<uint8_t> content)
{
    static_assert(std::is_base_of<ITextureBase, T>());

    // TODO: Should check size of data matches expected size

    const ImageDescription image_desc = TextureBase<T, DimensionsT>::get_image_description(desc);

    const std::shared_ptr<ImageResource> resource = ImageResource::create(image_desc);
    BufferUtils::initialize_image(*resource, content.data(), content.size());

    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
ImageDescription TextureBase<T, DimensionsT>::get_image_description(const Description& desc)
{
    ImageDescription image_desc{};
    if constexpr (DimensionsT::length() >= 1)
        image_desc.width = desc.dimensions.x;
    if constexpr (DimensionsT::length() >= 2)
        image_desc.height = desc.dimensions.y;
    if constexpr (DimensionsT::length() >= 3)
        image_desc.depth = desc.dimensions.z;

    const uint32_t max_num_mips = ImageUtils::compute_num_mips(image_desc.width, image_desc.height, image_desc.depth);
    if (desc.num_mips < 1 || desc.num_mips > max_num_mips)
    {
        MIZU_LOG_WARNING(
            "Invalid number of mips ({} when valid range is 1-{}), clamping to nearest valid",
            desc.num_mips,
            max_num_mips);
    }

    if (desc.num_layers < 1)
    {
        MIZU_LOG_WARNING("Invalid number of layers ({}), layer must be at least 1, will use 1", desc.num_layers);
    }

    if constexpr (DimensionsT::length() == 1)
        image_desc.type = ImageType::Image1D;
    else if constexpr (DimensionsT::length() == 2)
        image_desc.type = ImageType::Image2D;
    else if constexpr (DimensionsT::length() == 3)
        image_desc.type = ImageType::Image3D;

    image_desc.format = desc.format;
    image_desc.usage = desc.usage;
    image_desc.num_mips = glm::clamp(desc.num_mips, 1u, max_num_mips);
    image_desc.num_layers = glm::max(desc.num_layers, 1u);

    image_desc.is_virtual = desc.is_virtual;
    image_desc.is_aliased = desc.is_aliased;

    image_desc.name = desc.name;

    return image_desc;
}

template class TextureBase<Texture1D, glm::uvec1>;
template class TextureBase<Texture2D, glm::uvec2>;
template class TextureBase<Texture3D, glm::uvec3>;

} // namespace Mizu