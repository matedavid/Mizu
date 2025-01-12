#pragma once

#include <vector>

#include "render_core/rhi/command_buffer.h"

namespace Mizu::OpenGL
{

// Forward declarations
class OpenGLResourceGroup;
class OpenGLGraphicsPipeline;
class OpenGLComputePipeline;
class OpenGLShaderBase;

class OpenGLCommandBufferBase : public virtual ICommandBuffer
{
  public:
    OpenGLCommandBufferBase() = default;
    virtual ~OpenGLCommandBufferBase() = default;

    void begin() override {}
    void end() override {}

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_resource_group(const ResourceGroup& resource_group, uint32_t set) const override;

    void push_constant([[maybe_unused]] std::string_view name,
                       [[maybe_unused]] uint32_t size,
                       [[maybe_unused]] const void* data) const override
    {
    }

    void transition_resource(ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state) const override;

    void begin_debug_label(const std::string_view& label) const override;
    void end_debug_label() const override;

  protected:
    std::shared_ptr<OpenGLShaderBase> m_currently_bound_shader{};
};

//
// OpenGLRenderCommandBuffer
//

class OpenGLRenderCommandBuffer : public RenderCommandBuffer, public OpenGLCommandBufferBase
{
  public:
    OpenGLRenderCommandBuffer() = default;
    ~OpenGLRenderCommandBuffer() override = default;

    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;

    void begin_render_pass(const RenderPass& render_pass) const override;
    void end_render_pass(const RenderPass& render_pass) const override;

    void draw(const VertexBuffer& vertex) const override;
    void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const override;

    void dispatch(glm::uvec3 group_count) const override;

  private:
    std::shared_ptr<OpenGLGraphicsPipeline> m_bound_graphics_pipeline{nullptr};
    std::shared_ptr<OpenGLComputePipeline> m_bound_compute_pipeline{nullptr};
};

//
// OpenGLComputeCommandBuffer
//

class OpenGLComputeCommandBuffer : public ComputeCommandBuffer, public OpenGLCommandBufferBase
{
  public:
    OpenGLComputeCommandBuffer() = default;
    ~OpenGLComputeCommandBuffer() override = default;

    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;
    void dispatch(glm::uvec3 group_count) const override;

  private:
    std::shared_ptr<OpenGLComputePipeline> m_bound_pipeline{nullptr};
};

} // namespace Mizu::OpenGL
