#include "opengl_cubemap.h"

#include <stb_image.h>

#include "utility/assert.h"

namespace Mizu::OpenGL {

OpenGLCubemap::OpenGLCubemap(const ImageDescription& desc) {
    // TODO:
    MIZU_UNREACHABLE("Unimplemented");
}

OpenGLCubemap::OpenGLCubemap(const Faces& faces) {
    init();

    load_face(faces.right, GL_TEXTURE_CUBE_MAP_POSITIVE_X, m_width, m_height);
    load_face(faces.left, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, m_width, m_height);
    load_face(faces.top, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, m_width, m_height);
    load_face(faces.bottom, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, m_width, m_height);
    load_face(faces.front, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, m_width, m_height);
    load_face(faces.back, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, m_width, m_height);

    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
}

OpenGLCubemap::~OpenGLCubemap() {
    glDeleteTextures(1, &m_handle);
}

void OpenGLCubemap::init() {
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_handle);

    // TODO: int32_t min_filter = is_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;
    const GLint min_filter = GL_LINEAR;

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
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