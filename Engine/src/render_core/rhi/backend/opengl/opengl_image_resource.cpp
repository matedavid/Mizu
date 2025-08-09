#include "opengl_image_resource.h"

#include "base/debug/assert.h"
#include "render_core/rhi/backend/opengl/opengl_context.h"

namespace Mizu::OpenGL
{

OpenGLImageResource::OpenGLImageResource(const ImageDescription& desc) : m_description(desc)
{
    const auto [internal, format, type] = OpenGLImageResource::get_format_info(desc.format);

    const uint32_t num_components = OpenGLImageResource::get_num_components(format);
    const uint32_t type_size = OpenGLImageResource::get_type_size(type);

    std::vector<uint8_t> data;
    switch (desc.type)
    {
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

    init(data.data());
};

OpenGLImageResource::OpenGLImageResource(const ImageDescription& desc, const uint8_t* data) : m_description(desc)
{
    init(data);
}

OpenGLImageResource::~OpenGLImageResource()
{
    glDeleteTextures(1, &m_handle);
}

GLenum OpenGLImageResource::get_image_type(ImageType type)
{
    switch (type)
    {
    case ImageType::Image1D:
        return GL_TEXTURE_1D;
    case ImageType::Image2D:
        return GL_TEXTURE_2D;
    case ImageType::Image3D:
        return GL_TEXTURE_3D;
    case ImageType::Cubemap:
        return GL_TEXTURE_CUBE_MAP;
    }
}

/*
GLint OpenGLImageResource::get_filter(ImageFilter filter)
{
    switch (filter)
    {
    case ImageFilter::Linear:
        return GL_LINEAR;
    case ImageFilter::Nearest:
        return GL_NEAREST;
    }
}

GLint OpenGLImageResource::get_sampler_address_mode(ImageAddressMode mode)
{
    switch (mode)
    {
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
*/

uint32_t OpenGLImageResource::get_type_size(GLuint type)
{
    switch (type)
    {
    case GL_UNSIGNED_BYTE:
        return 1;
    case GL_FLOAT:
        return 4;
    default:
        MIZU_UNREACHABLE("OpenGL get_type_size type not implemented");
    }

    return 0; // Default to prevent compilation errors
}

uint32_t OpenGLImageResource::get_num_components(GLuint format)
{
    switch (format)
    {
    case GL_DEPTH_COMPONENT:
        return 1;
    case GL_RGBA:
    case GL_BGRA:
        return 4;
    default:
        MIZU_UNREACHABLE("OpenGL num_components format not implemented");
    }

    return 0; // Default to prevent compilation errors
}

std::tuple<GLint, GLuint, GLuint> OpenGLImageResource::get_format_info(ImageFormat format)
{
    /*
    switch (format)
    {
    case ImageFormat::RGBA8_SRGB:
        return {GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case ImageFormat::RGBA8_UNORM:
        return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    case ImageFormat::RGBA16_SFLOAT:
        return {GL_RGBA16F, GL_RGBA, GL_FLOAT};
    case ImageFormat::RGBA32_SFLOAT:
        return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
    case ImageFormat::BGRA8_SRGB:
        return {GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE};
    case ImageFormat::D32_SFLOAT:
        return {GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT};
    }
    */

    (void)format;
    MIZU_UNREACHABLE("Unimplemented");
    return {};
}

void OpenGLImageResource::init(const uint8_t* data)
{
    glGenTextures(1, &m_handle);

    const GLenum image_type = get_image_type(m_description.type);
    glBindTexture(image_type, m_handle);

    // TODO: Should change minification filter if is_mipmap_enabled
    // GLint min_filter = is_mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR;

    /*
    glTextureParameteri(m_handle, GL_TEXTURE_MIN_FILTER, get_filter(m_sampling_options.minification_filter));
    glTextureParameteri(m_handle, GL_TEXTURE_MAG_FILTER, get_filter(m_sampling_options.magnification_filter));

    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_S, get_sampler_address_mode(m_sampling_options.address_mode_u));
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_T, get_sampler_address_mode(m_sampling_options.address_mode_v));
    glTextureParameteri(m_handle, GL_TEXTURE_WRAP_R, get_sampler_address_mode(m_sampling_options.address_mode_w));
    */

    switch (m_description.type)
    {
    case ImageType::Image1D:
        initialize_image1d(data);
        break;
    case ImageType::Image2D:
        initialize_image2d(data);
        break;
    case ImageType::Image3D:
        initialize_image3d(data);
        break;
    case ImageType::Cubemap:
        initialize_cubemap(data);
        break;
    }

    glBindTexture(image_type, 0);

    if (!m_description.name.empty())
    {
        GL_DEBUG_SET_OBJECT_NAME(GL_TEXTURE, m_handle, m_description.name);
    }
}

void OpenGLImageResource::initialize_image1d(const uint8_t* data) const
{
    const auto [internal, format, type] = get_format_info(m_description.format);

    // const size_t expected_size = m_description.width * get_num_components(format) * get_type_size(type);
    // MIZU_ASSERT(expected_size == data.size(),
    //            "Expected data size does not match provided data size ({} != {})",
    //            expected_size,
    //            data.size());

    glTexImage1D(GL_TEXTURE_1D, 0, internal, static_cast<GLint>(m_description.width), 0, format, type, data);
}

void OpenGLImageResource::initialize_image2d(const uint8_t* data) const
{
    const auto [internal, format, type] = get_format_info(m_description.format);

    // const size_t expected_size =
    //     m_description.width * m_description.height * get_num_components(format) * get_type_size(type);
    // MIZU_ASSERT(expected_size == data.size(),
    //             "Expected data size does not match provided data size ({} != {})",
    //             expected_size,
    //             data.size());

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal,
        static_cast<GLint>(m_description.width),
        static_cast<GLint>(m_description.height),
        0,
        format,
        type,
        data);
}

void OpenGLImageResource::initialize_image3d(const uint8_t* data) const
{
    const auto [internal, format, type] = get_format_info(m_description.format);

    // const size_t expected_size = m_description.width * m_description.height * m_description.depth
    //                              * get_num_components(format) * get_type_size(type);
    // MIZU_ASSERT(expected_size == data.size(),
    //             "Expected data size does not match provided data size ({} != {})",
    //             expected_size,
    //             data.size());

    glTexImage3D(
        GL_TEXTURE_3D,
        0,
        internal,
        static_cast<GLint>(m_description.width),
        static_cast<GLint>(m_description.height),
        static_cast<GLint>(m_description.depth),
        0,
        format,
        type,
        data);
}

void OpenGLImageResource::initialize_cubemap(const uint8_t* data) const
{
    // MIZU_ASSERT(data.size() % 6 == 0, "Size of data for cubemap should be divisible by 6");

    const uint32_t size = m_description.width * m_description.height * m_description.depth
                          * ImageUtils::get_format_size(m_description.format) * 6;

    const size_t size_per_page = size / 6;
    for (uint32_t i = 0; i < 6; ++i)
    {
        const uint8_t* ptr = data + i * size_per_page;
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
            0,
            GL_RGBA,
            static_cast<int32_t>(m_description.width),
            static_cast<int32_t>(m_description.height),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            ptr);
    }
}

} // namespace Mizu::OpenGL
