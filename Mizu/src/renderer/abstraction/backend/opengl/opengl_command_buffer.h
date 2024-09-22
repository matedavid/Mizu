#pragma once

#include <unordered_map>

#include "renderer/abstraction/command_buffer.h"

namespace Mizu::OpenGL {

// Forward declarations
class OpenGLResourceGroup;
class OpenGLGraphicsPipeline;
class OpenGLComputePipeline;
class OpenGLShaderBase;

class OpenGLCommandBufferBase : public virtual ICommandBuffer {
  public:
    OpenGLCommandBufferBase() = default;
    virtual ~OpenGLCommandBufferBase() = default;

    void begin() override {}
    void end() override;

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant([[maybe_unused]] std::string_view name,
                       [[maybe_unused]] uint32_t size,
                       [[maybe_unused]] const void* data) override {}

  protected:
    std::unordered_map<uint32_t, std::shared_ptr<OpenGLResourceGroup>> m_bound_resources;

    void bind_bound_resources(const std::shared_ptr<OpenGLShaderBase>& shader) const;
};

//
// OpenGLRenderCommandBuffer
//

class OpenGLRenderCommandBuffer : public RenderCommandBuffer, public OpenGLCommandBufferBase {
  public:
    OpenGLRenderCommandBuffer() = default;
    ~OpenGLRenderCommandBuffer() override = default;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) override;

    void bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) override;

    void begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;
    void end_render_pass(const std::shared_ptr<RenderPass>& render_pass) override;

    void draw(const std::shared_ptr<VertexBuffer>& vertex) override;
    void draw_indexed(const std::shared_ptr<VertexBuffer>& vertex, const std::shared_ptr<IndexBuffer>& index) override;

  private:
    std::shared_ptr<OpenGLGraphicsPipeline> m_bound_pipeline{nullptr};
};

//
// OpenGLComputeCommandBuffer
//

class OpenGLComputeCommandBuffer : public ComputeCommandBuffer, public OpenGLCommandBufferBase {
  public:
    OpenGLComputeCommandBuffer() = default;
    ~OpenGLComputeCommandBuffer() override = default;

    void bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group, uint32_t set) override;
    void push_constant(std::string_view name, uint32_t size, const void* data) override;

  private:
    std::shared_ptr<OpenGLComputePipeline> m_bound_pipeline{nullptr};
};

} // namespace Mizu::OpenGL
