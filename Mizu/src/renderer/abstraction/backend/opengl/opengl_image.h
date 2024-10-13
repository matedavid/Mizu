#pragma once

#include <glad/glad.h>
#include <tuple>

#include "renderer/abstraction/image.h"

namespace Mizu::OpenGL {

class OpenGLImage : public virtual IImage {
  public:
    struct Description {
        uint32_t width = 1, height = 1, depth = 1;
        ImageFormat format = ImageFormat::RGBA8_SRGB;
        GLenum type = GL_TEXTURE_2D;
        ImageUsageBits usage = ImageUsageBits::None;

        /*
        uint32_t num_mips = 1;
        uint32_t num_layers = 1;
        */
    };

    void init_resources(const Description& desc, const SamplingOptions& sampling);

    ~OpenGLImage() override;

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }
    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] ImageUsageBits get_usage() const override { return m_description.usage; }

    [[nodiscard]] static GLint get_filter(ImageFilter filter);
    [[nodiscard]] static GLint get_sampler_address_mode(ImageAddressMode mode);
    [[nodiscard]] static uint32_t get_type_size(GLuint type);
    [[nodiscard]] static uint32_t get_num_components(GLuint format);

    // internal, format, type
    [[nodiscard]] static std::tuple<GLint, GLuint, GLuint> get_format_info(ImageFormat format);

    [[nodiscard]] GLuint handle() const { return m_handle; }

  protected:
    OpenGLImage() = default;

    GLuint m_handle{};

    Description m_description{};
    SamplingOptions m_sampling{};
};

} // namespace Mizu::OpenGL