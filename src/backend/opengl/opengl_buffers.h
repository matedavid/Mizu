#pragma once

#include <glad/glad.h>
#include <vector>

#include "buffers.h"

namespace Mizu::OpenGL {

class OpenGLUniformBuffer : public UniformBuffer {
  public:
    explicit OpenGLUniformBuffer(uint32_t size);
    ~OpenGLUniformBuffer() override;

    void set_data(const void* data) override;
    void set_data(const void* data, uint32_t size, uint32_t offset_bytes) override;

    [[nodiscard]] uint32_t size() const override { return m_size; }

    [[nodiscard]] uint32_t handle() const { return m_handle; }

  private:
    uint32_t m_handle{};
    uint32_t m_size;
};

} // namespace Mizu::OpenGL
