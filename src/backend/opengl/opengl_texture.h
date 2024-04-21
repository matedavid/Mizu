#pragma once

#include <glad/glad.h>
#include <tuple>

#include "texture.h"

namespace Mizu::OpenGL {

class OpenGLTexture2D : public Texture2D {
  public:
    explicit OpenGLTexture2D(const ImageDescription& desc);
    ~OpenGLTexture2D() override;

    [[nodiscard]] ImageFormat get_format() const override;
    [[nodiscard]] uint32_t get_width() const override;
    [[nodiscard]] uint32_t get_height() const override;

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    ImageDescription m_description;

    [[nodiscard]] static std::tuple<GLint, GLuint, GLuint> get_format(ImageFormat format);
    [[nodiscard]] static GLint get_filter(ImageFilter filter);
    [[nodiscard]] static GLint get_sampler_address_mode(ImageAddressMode mode);

    [[nodiscard]] static uint32_t get_type_size(GLuint type);
    [[nodiscard]] static uint32_t get_num_components(GLuint format);
};

} // namespace Mizu::OpenGL
