#pragma once

#include "render_core/rhi/renderer.h"

// Forward declarations
struct GLFWwindow;

namespace Mizu::OpenGL
{

class OpenGLBackend : public IBackend
{
  public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override;

    bool initialize(const RendererConfiguration& config) override;

    void wait_idle() const override;

  private:
    GLFWwindow* m_offscreen_window = nullptr;
};

} // namespace Mizu::OpenGL
