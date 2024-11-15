#include "opengl_device_memory_allocator.h"

#include "renderer/abstraction/backend/opengl/opengl_image_resource.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

std::shared_ptr<ImageResource> OpenGLBaseDeviceMemoryAllocator::allocate_image_resource(
    const ImageDescription& desc,
    const SamplingOptions& sampling) {
    const auto [internal, format, type] = OpenGLImageResource::get_format_info(desc.format);

    const uint32_t num_components = OpenGLImageResource::get_num_components(format);
    const uint32_t type_size = OpenGLImageResource::get_type_size(type);

    std::vector<uint8_t> data;
    switch (desc.type) {
    case ImageType::Image1D:
        data = std::vector<uint8_t>(desc.width * num_components * type_size, 0);
        break;
    case ImageType::Image2D:
        data = std::vector<uint8_t>(desc.width * desc.height * num_components * type_size, 0);
        break;
    case ImageType::Image3D:
        data = std::vector<uint8_t>(desc.width * desc.height * desc.depth * num_components * type_size, 0);
        break;
    case ImageType::Cubemap:
        data = std::vector<uint8_t>(desc.width * desc.height * num_components * type_size * 6, 0);
        break;
    }

    return allocate_image_resource(desc, sampling, data.data(), data.size());
}

std::shared_ptr<ImageResource> OpenGLBaseDeviceMemoryAllocator::allocate_image_resource(const ImageDescription& desc,
                                                                                        const SamplingOptions& sampling,
                                                                                        const uint8_t* data,
                                                                                        uint32_t size) {
    std::vector<uint8_t> vec_data(data, data + size);
    return std::make_shared<OpenGLImageResource>(desc, sampling, vec_data);
}

void OpenGLBaseDeviceMemoryAllocator::release(const std::shared_ptr<ImageResource>& resource) {}

} // namespace Mizu::OpenGL