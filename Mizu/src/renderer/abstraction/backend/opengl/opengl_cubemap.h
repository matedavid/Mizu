#pragma once

#include <filesystem>
#include <glad/glad.h>

#include "renderer/abstraction/cubemap.h"

#include "utility/logging.h"

namespace Mizu::OpenGL {

class OpenGLCubemap : public Cubemap {
  public:
    OpenGLCubemap(const ImageDescription& desc);
    OpenGLCubemap(const Faces& faces);

    ~OpenGLCubemap() override;

    [[nodiscard]] uint32_t get_width() const override { return m_width; }
    [[nodiscard]] uint32_t get_height() const override { return m_height; }
    [[nodiscard]] ImageFormat get_format() const override {
        MIZU_LOG_ERROR("Value is wrong");
        return ImageFormat::RGBA8_SRGB;
    }
    [[nodiscard]] ImageUsageBits get_usage() const override {
        MIZU_LOG_ERROR("Value is wrong");
        return ImageUsageBits::Sampled;
    }

    [[nodiscard]] GLuint handle() const { return m_handle; }

  private:
    GLuint m_handle{};
    uint32_t m_width, m_height;

    void init();
    void load_face(const std::filesystem::path& path, uint32_t face, uint32_t& width, uint32_t& height) const;
};

} // namespace Mizu::OpenGL