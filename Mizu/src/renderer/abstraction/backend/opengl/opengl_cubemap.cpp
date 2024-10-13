#include "opengl_cubemap.h"

#include <stb_image.h>

#include "utility/assert.h"

namespace Mizu::OpenGL {

OpenGLCubemap::OpenGLCubemap(const ImageDescription& desc) {
    // TODO:
    MIZU_UNREACHABLE("Unimplemented");
}

OpenGLCubemap::OpenGLCubemap(const Faces& faces) {
    OpenGLImage::Description image_desc{};
    image_desc.depth = 1;
    image_desc.format = ImageFormat::RGBA8_SRGB;
    image_desc.type = GL_TEXTURE_CUBE_MAP;
    image_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::Storage;

    init_resources(image_desc, SamplingOptions{});

    uint32_t width, height;

    load_face(faces.right, GL_TEXTURE_CUBE_MAP_POSITIVE_X, width, height);
    load_face(faces.left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, width, height);
    load_face(faces.top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, width, height);
    load_face(faces.bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, width, height);
    load_face(faces.front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, width, height);
    load_face(faces.back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, width, height);

    m_description.width = width;
    m_description.height = height;

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

void OpenGLCubemap::load_face(const std::filesystem::path& path,
                              uint32_t face,
                              uint32_t& width,
                              uint32_t& height) const {
    const auto extension = path.extension();

    int32_t width_local, height_local, num_channels;
    auto* data = stbi_load(path.string().c_str(), &width_local, &height_local, &num_channels, 0);
    MIZU_ASSERT(data, "Failed to load cubemap face with path: {}", path.string());

    if (face == GL_TEXTURE_CUBE_MAP_POSITIVE_X) {
        width = static_cast<uint32_t>(width_local);
        height = static_cast<uint32_t>(height_local);
    } else {
        MIZU_ASSERT(width == static_cast<uint32_t>(width_local) && height == static_cast<uint32_t>(height_local),
                    "Width and Height values of face {} do not match with previous faces",
                    path.string());
    }

    glTexImage2D(face, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);
}

} // namespace Mizu::OpenGL