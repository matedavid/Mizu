#pragma once

#include "command_buffer.h"

namespace Mizu::OpenGL {

class OpenGLRenderCommandBuffer : public RenderCommandBuffer {
  public:
    OpenGLRenderCommandBuffer() = default;
    ~OpenGLRenderCommandBuffer() override = default;

    void begin() const override {}
    void end() const override {}

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;
};

class OpenGLComputeCommandBuffer : public ComputeCommandBuffer {
  public:
    OpenGLComputeCommandBuffer() = default;
    ~OpenGLComputeCommandBuffer() override = default;

    void begin() const override {}
    void end() const override {}

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}
};

} // namespace Mizu::OpenGL
