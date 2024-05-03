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

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;
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
