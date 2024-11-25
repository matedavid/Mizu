#include "texture.h"

namespace Mizu {

template class TextureBase<Texture1D, glm::uvec1>;
template class TextureBase<Texture2D, glm::uvec2>;
template class TextureBase<Texture3D, glm::uvec3>;

template <typename T, typename DimensionsT>
std::shared_ptr<T> TextureBase<T, DimensionsT>::create(const Description& desc,
                                                       const SamplingOptions& sampling,
                                                       std::weak_ptr<IDeviceMemoryAllocator> allocator) {
    static_assert(std::is_base_of<ITextureBase, T>());

    const ImageDescription image_desc = TextureBase<T, DimensionsT>::get_image_description(desc);
    const std::shared_ptr<ImageResource> resource = ImageResource::create(image_desc, sampling, allocator);
    return std::make_shared<T>(resource);
}

template <typename T, typename DimensionsT>
ImageDescription TextureBase<T, DimensionsT>::get_image_description(const Description& desc) {
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

    return image_desc;
}

} // namespace Mizu