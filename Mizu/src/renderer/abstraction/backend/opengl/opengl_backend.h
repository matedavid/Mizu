#pragma once

#include "renderer/abstraction/renderer.h"

namespace Mizu::OpenGL {

class OpenGLBackend : public IBackend {
  public:
    OpenGLBackend() = default;
    ~OpenGLBackend() override = default;

    bool initialize(const RendererConfiguration& config) override;
};

} // namespace Mizu::OpenGL
