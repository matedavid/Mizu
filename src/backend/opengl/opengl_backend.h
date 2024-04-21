#pragma once

#include "renderer.h"

#include <EGL/egl.h>

namespace Mizu::OpenGL {

class OpenGLBackend : public IBackend {
  public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    bool initialize(const Configuration& config) override;

    void draw(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const override;
    void draw_indexed(const std::shared_ptr<ICommandBuffer>& command_buffer, uint32_t count) const override;

  private:
    EGLDisplay m_display{};
    EGLContext m_context{};
};

} // namespace Mizu::OpenGL
