#pragma once

#include <unordered_map>

#include "command_buffer.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLResourceGroup;
class OpenGLGraphicsPipeline;
class OpenGLShader;

class OpenGLCommandBufferBase {
  public:
    OpenGLCommandBufferBase() = default;
    virtual ~OpenGLCommandBufferBase() = default;

    void end_base();

    void bind_resource_group_base(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set);

  protected:
    std::unordered_map<uint32_t, std::shared_ptr<OpenGLResourceGroup>> m_bound_resources;
};

class OpenGLRenderCommandBuffer : public RenderCommandBuffer, public OpenGLCommandBufferBase {
  public:
    OpenGLRenderCommandBuffer() = default;
    ~OpenGLRenderCommandBuffer() override = default;

    void begin() override {}
    void end() override { end_base(); }

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;

  private:
    std::shared_ptr<OpenGLGraphicsPipeline> m_bound_pipeline{nullptr};

    void bind_bound_resources(const std::shared_ptr<OpenGLShader>& shader) const;
};

class OpenGLComputeCommandBuffer : public ComputeCommandBuffer, public OpenGLCommandBufferBase {
  public:
    OpenGLComputeCommandBuffer() = default;
    ~OpenGLComputeCommandBuffer() override = default;

    void begin() override {}
    void end() override { end_base(); }

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override {
        bind_resource_group_base(resource_group, set);
    }
};

} // namespace Mizu::OpenGL
