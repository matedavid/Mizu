#pragma once

#include "renderer/abstraction/renderer.h"

#include <EGL/egl.h>

namespace Mizu::OpenGL {

class OpenGLBackend : public IBackend {
  public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    bool initialize(const RendererConfiguration& config) override;

  private:
    EGLDisplay m_display{};
    EGLContext m_context{};
};

} // namespace Mizu::OpenGL
