#include "opengl_image.h"

namespace Mizu::OpenGL {

void OpenGLImage::init_resources(const Description& desc, const SamplingOptions& sampling) {
    m_description = desc;
    m_sampling = sampling;

    glGenTextures(1, &m_handle);
    glBindTexture(desc.type, m_handle);

    const auto [internal, format, type] = get_format_info(m_description.format);

    // TODO: Should change minification filter if is_mipmap_enabled
    // GLint min_filter = is_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;

    glTexParameteri(desc.type, GL_TEXTURE_MIN_FILTER, get_filter(m_sampling.minification_filter));
    glTextureParameteri(desc.type, GL_TEXTURE_MAG_FILTER, get_filter(m_sampling.magnification_filter));

    glTextureParameteri(desc.type, GL_TEXTURE_WRAP_S, get_sampler_address_mode(m_sampling.address_mode_u));
    glTextureParameteri(desc.type, GL_TEXTURE_WRAP_T, get_sampler_address_mode(m_sampling.address_mode_v));
    glTextureParameteri(desc.type, GL_TEXTURE_WRAP_R, get_sampler_address_mode(m_sampling.address_mode_w));
}

OpenGLImage::~OpenGLImage() {
    glDeleteTextures(1, &m_handle);
}

GLint OpenGLImage::get_filter(ImageFilter filter) {
    switch (filter) {
    case ImageFilter::Linear:
        return GL_LINEAR;
    case ImageFilter::Nearest:
        return GL_NEAREST;
    }
}

GLint OpenGLImage::get_sampler_address_mode(ImageAddressMode mode) {
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

uint32_t OpenGLImage::get_type_size(GLuint type) {
    switch (type) {
    default:
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_FLOAT:
        return 4;
    }
}

uint32_t OpenGLImage::get_num_components(GLuint format) {
    switch (format) {
    default:
    case GL_DEPTH_COMPONENT:
        return 1;
    case GL_RGBA:
    case GL_BGRA:
        return 4;
    }
}

std::tuple<GLint, GLuint, GLuint> OpenGLImage::get_format_info(ImageFormat format) {
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

} // namespace Mizu::OpenGL