#pragma once

#include "command_buffer.h"

namespace Mizu::OpenGL {

class OpenGLCommandBuffer : public RenderCommandBuffer, public ComputeCommandBuffer {
  public:
    OpenGLCommandBuffer() = default;
    ~OpenGLCommandBuffer() override = default;

    void begin() const override;
    void end() const override;

    void submit() const override;
    void submit(const CommandBufferSubmitInfo& info) const override;
};

} // namespace Mizu::OpenGL
