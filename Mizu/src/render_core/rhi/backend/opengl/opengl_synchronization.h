#pragma once

#include "render_core/rhi/synchronization.h"

namespace Mizu::OpenGL
{

class OpenGLFence : public Fence
{
  public:
    OpenGLFence() = default;
    ~OpenGLFence() override = default;

    void wait_for() const override;
};

class OpenGLSemaphore : public Semaphore
{
  public:
    OpenGLSemaphore() = default;
    ~OpenGLSemaphore() override = default;
};

} // namespace Mizu::OpenGL
