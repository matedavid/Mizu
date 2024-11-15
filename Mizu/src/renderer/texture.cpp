#include "texture.h"

namespace Mizu {

template class TextureBase<glm::uvec1>; // Texture1D
template class TextureBase<glm::uvec2>; // Texture2D
template class TextureBase<glm::uvec3>; // Texture3D

template <typename T>
TextureBase<T>::TextureBase(std::shared_ptr<ImageResource> resource) : m_resource(std::move(resource)) {}

template <typename T>
ImageDescription TextureBase<T>::get_image_description(const Description& desc) {
    ImageDescription image_desc{};
    if constexpr (T::length() >= 1)
        image_desc.width = desc.dimensions.x;
    if constexpr (T::length() >= 2)
        image_desc.height = desc.dimensions.y;
    if constexpr (T::length() >= 3)
        image_desc.depth = desc.dimensions.z;

    image_desc.format = desc.format;
    image_desc.usage = desc.usage;
    image_desc.num_mips =
        desc.generate_mips ? ImageUtils::compute_num_mips(image_desc.width, image_desc.height, image_desc.depth) : 1;
    image_desc.num_layers = 1;

    return image_desc;
}

} // namespace Mizu