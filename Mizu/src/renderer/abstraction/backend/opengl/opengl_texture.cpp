#include "opengl_texture.h"

#include <vector>

namespace Mizu::OpenGL {

OpenGLTexture2D::OpenGLTexture2D(const ImageDescription& desc) : m_description(desc) {
    glGenTextures(1, &m_handle);
    glBindTexture(GL_TEXTURE_2D, m_handle);

    const auto [internal, format, type] = get_format(m_description.format);
    std::vector<char> data(
        m_description.width * m_description.height * get_num_components(format) * get_type_size(type), 0);

    glTexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, get_filter(m_description.sampling_options.minification_filter));
    glTextureParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, get_filter(m_description.sampling_options.magnification_filter));

    glTextureParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, get_sampler_address_mode(m_description.sampling_options.address_mode_u));
    glTextureParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, get_sampler_address_mode(m_description.sampling_options.address_mode_v));
    glTextureParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, get_sampler_address_mode(m_description.sampling_options.address_mode_w));

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 internal,
                 static_cast<GLint>(m_description.width),
                 static_cast<GLint>(m_description.height),
                 0,
                 format,
                 type,
                 data.data());

    if (m_description.generate_mips) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

OpenGLTexture2D::~OpenGLTexture2D() {
    glDeleteTextures(1, &m_handle);
}

std::tuple<GLint, GLuint, GLuint> OpenGLTexture2D::get_format(ImageFormat format) {
    switch (format) {
    case ImageFormat::RGBA8_SRGB:
        return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case ImageFormat::RGBA8_UNORM:
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case ImageFormat::RGBA16_SFLOAT:
        return {GL_RGBA16F, GL_RGBA, GL_FLOAT};
    case ImageFormat::BGRA8_SRGB:
        return {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case ImageFormat::D32_SFLOAT:
        return {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT};
    }
}

GLint OpenGLTexture2D::get_filter(ImageFilter filter) {
    switch (filter) {
    case ImageFilter::Linear:
        return GL_LINEAR;
    case ImageFilter::Nearest:
        return GL_NEAREST;
    }
}

GLint OpenGLTexture2D::get_sampler_address_mode(ImageAddressMode mode) {
    switch (mode) {
    case ImageAddressMode::Repeat:
        return GL_REPEAT;
    case ImageAddressMode::MirroredRepeat:
        return GL_MIRRORED_REPEAT;
    case ImageAddressMode::ClampToEdge:
        return GL_CLAMP_TO_EDGE;
    case ImageAddressMode::ClampToBorder:
        return GL_CLAMP_TO_BORDER;
    }
}

uint32_t OpenGLTexture2D::get_type_size(GLuint type) {
    switch (type) {
    default:
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_FLOAT:
        return 4;
    }
}

uint32_t OpenGLTexture2D::get_num_components(GLuint format) {
    switch (format) {
    default:
    case GL_DEPTH_COMPONENT:
        return 1;
    case GL_RGBA:
    case GL_BGRA:
        return 4;
    }
}

} // namespace Mizu::OpenGL
