#pragma once

#include <glad/glad.h>
#include <tuple>
#include <vector>

#include "render_core/rhi/image_resource.h"

namespace Mizu::OpenGL
{

class OpenGLImageResource : public ImageResource
{
  public:
    OpenGLImageResource(const ImageDescription& desc);
    OpenGLImageResource(const ImageDescription& desc, const std::vector<uint8_t>& data);
    ~OpenGLImageResource() override;

    [[nodiscard]] uint32_t get_width() const override { return m_description.width; }
    [[nodiscard]] uint32_t get_height() const override { return m_description.height; }
    [[nodiscard]] uint32_t get_depth() const override { return m_description.depth; }
    [[nodiscard]] ImageType get_image_type() const override { return m_description.type; }
    [[nodiscard]] ImageFormat get_format() const override { return m_description.format; }
    [[nodiscard]] ImageUsageBits get_usage() const override { return m_description.usage; }
    [[nodiscard]] uint32_t get_num_mips() const override { return m_description.num_mips; }
    [[nodiscard]] uint32_t get_num_layers() const override { return m_description.num_layers; }

    [[nodiscard]] static GLenum get_image_type(ImageType type);
    /*
    [[nodiscard]] static GLint get_filter(ImageFilter filter);
    [[nodiscard]] static GLint get_sampler_address_mode(ImageAddressMode mode);
    */
    [[nodiscard]] static uint32_t get_type_size(GLuint type);
    [[nodiscard]] static uint32_t get_num_components(GLuint format);

    // internal, format, type
    [[nodiscard]] static std::tuple<GLint, GLuint, GLuint> get_format_info(ImageFormat format);

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle;

    ImageDescription m_description;

    void init(const std::vector<uint8_t>& data);

    void initialize_image1d(const std::vector<uint8_t>& data) const;
    void initialize_image2d(const std::vector<uint8_t>& data) const;
    void initialize_image3d(const std::vector<uint8_t>& data) const;
    void initialize_cubemap(const std::vector<uint8_t>& data) const;
};

} // namespace Mizu::OpenGL