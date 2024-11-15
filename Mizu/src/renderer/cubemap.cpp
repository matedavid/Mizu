#include "cubemap.h"

namespace Mizu {

Cubemap::Cubemap(std::shared_ptr<ImageResource> resource) : m_resource(std::move(resource)) {}

ImageDescription Cubemap::get_image_description(const Description& desc) {
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