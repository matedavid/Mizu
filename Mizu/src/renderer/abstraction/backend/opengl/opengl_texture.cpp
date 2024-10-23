#include "opengl_texture.h"

#include <stb_image.h>
#include <vector>

#include "utility/assert.h"

namespace Mizu::OpenGL {

OpenGLTexture2D::OpenGLTexture2D(const ImageDescription& desc) {
    OpenGLImage::Description image_desc{};
    image_desc.width = desc.width;
    image_desc.height = desc.height;
    image_desc.depth = 1;
    image_desc.format = desc.format;
    image_desc.type = GL_TEXTURE_2D;
    image_desc.usage = desc.usage;

    init_resources(image_desc, desc.sampling_options);

    const auto& [internal, format, type] = get_format_info(m_description.format);
    std::vector<char> data(
        m_description.width * m_description.height * get_num_components(format) * get_type_size(type), 0);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 internal,
                 static_cast<GLint>(m_description.width),
                 static_cast<GLint>(m_description.height),
                 0,
                 format,
                 type,
                 data.data());

    // TODO: Think how to treat mips
    /*
    if (desc.generate_mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    */

    glBindTexture(GL_TEXTURE_2D, 0);
}

OpenGLTexture2D::OpenGLTexture2D(const std::filesystem::path& path, SamplingOptions sampling) {
    const auto& str_path = path.string();
    MIZU_ASSERT(std::filesystem::exists(path), "Texture with path {} does not exist", str_path);

    int32_t width, height, channels;
    const stbi_uc* pixels = stbi_load(str_path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    MIZU_ASSERT(pixels != nullptr, "Could not open texture with path {}", str_path);

    OpenGLImage::Description image_desc{};
    image_desc.width = static_cast<uint32_t>(width);
    image_desc.height = static_cast<uint32_t>(height);
    image_desc.depth = 1;
    image_desc.format = ImageFormat::RGBA8_SRGB;
    image_desc.type = GL_TEXTURE_2D;
    image_desc.usage = ImageUsageBits::Sampled | ImageUsageBits::TransferDst;

    init_resources(image_desc, sampling);

    const auto& [internal, format, type] = get_format_info(m_description.format);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 internal,
                 static_cast<GLint>(m_description.width),
                 static_cast<GLint>(m_description.height),
                 0,
                 format,
                 type,
                 pixels);

    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace Mizu::OpenGL
