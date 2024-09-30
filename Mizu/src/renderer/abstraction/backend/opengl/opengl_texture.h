#pragma once

#include <glad/glad.h>
#include <tuple>

#include "renderer/abstraction/texture.h"

namespace Mizu::OpenGL {

class OpenGLTexture2D : public Texture2D {
  public:
    explicit OpenGLTexture2D(const ImageDescription& desc);
    ~OpenGLTexture2D() override;

    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }
    [[nodiscard]] ImageUsageBits get_usage() const override { return m_description.usage; }

    [[nodiscard]] ImageResourceState get_resource_state() const override {
        // TODO:: Implement
        return ImageResourceState::Undefined;
    }

    [[nodiscard]] GLuint handle() const { return m_handle; }

    // internal, format, type
    [[nodiscard]] static std::tuple<GLint, GLuint, GLuint> get_format(ImageFormat format);

  private:
    GLuint m_handle{};
    ImageDescription m_description;

    [[nodiscard]] static GLint get_filter(ImageFilter filter);
    [[nodiscard]] static GLint get_sampler_address_mode(ImageAddressMode mode);

    [[nodiscard]] static uint32_t get_type_size(GLuint type);
    [[nodiscard]] static uint32_t get_num_components(GLuint format);
};

} // namespace Mizu::OpenGL
