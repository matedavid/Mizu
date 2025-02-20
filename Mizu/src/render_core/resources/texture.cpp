#include "texture.h"

#include <cstring>
#include <stb_image.h>

#include "utility/assert.h"

namespace Mizu
{

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const Description& desc,
                                                       const SamplingOptions& sampling,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    static_assert(std::is_base_of<ITextureBase, T>());

    const ImageDescription image_desc = TextureBase<T, DimensionsT>::get_image_description(desc);
    const std::shared_ptr<ImageResource> resource = ImageResource::create(image_desc, sampling, allocator);
    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const std::filesystem::path& path,
                                                       const SamplingOptions& sampling,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator)
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
    desc.num_mips = 1; // TODO: Make configurable...
    desc.num_layers = 1;

    const uint32_t size = desc.width * desc.height * 4;

    std::vector<uint8_t> content(size);
    memcpy(content.data(), content_raw, size);

    const auto resource = ImageResource::create(desc, sampling, content, allocator);

    stbi_image_free(content_raw);

    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const Description& desc,
                                                       const SamplingOptions& sampling,
                                                       const std::vector<uint8_t>& content,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator)
{
    static_assert(std::is_base_of<ITextureBase, T>());

    // TODO: Should check size of data matches expected size

    const ImageDescription image_desc = TextureBase<T, DimensionsT>::get_image_description(desc);
    const std::shared_ptr<ImageResource> resource = ImageResource::create(image_desc, sampling, content, allocator);
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

    image_desc.format = desc.format;
    image_desc.usage = desc.usage;
    image_desc.num_mips =
        desc.generate_mips ? ImageUtils::compute_num_mips(image_desc.width, image_desc.height, image_desc.depth) : 1;
    image_desc.num_layers = 1;

    image_desc.name = desc.name;

    return image_desc;
}

template class TextureBase<Texture1D, glm::uvec1>;
template class TextureBase<Texture2D, glm::uvec2>;
template class TextureBase<Texture3D, glm::uvec3>;

} // namespace Mizu