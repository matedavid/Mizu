#pragma once

#include <vector>

#include "render_core/rhi/command_buffer.h"

#include "render_core/rhi/backend/opengl/opengl_render_pass.h"

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
    void end() override;

    void submit() const override {}
    void submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const override {}

    void bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set) override;

    void push_constant([[maybe_unused]] std::string_view name,
                       [[maybe_unused]] uint32_t size,
                       [[maybe_unused]] const void* data) const override
    {
    }

    void transition_resource(const ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state) const override;
    void transition_resource(const ImageResource& image,
                             ImageResourceState old_state,
                             ImageResourceState new_state,
                             ImageResourceViewRange range) const override;

    void copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const override;
    void copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const override;

    void begin_debug_label(const std::string_view& label) const override;
    void end_debug_label() const override;

  protected:
    std::shared_ptr<OpenGLShaderBase> m_currently_bound_shader{};

    struct CommandBufferResourceGroup
    {
        std::shared_ptr<OpenGLResourceGroup> resource_group = nullptr;
        bool is_bound = false;
    };
    std::vector<CommandBufferResourceGroup> m_resources;

    void bind_resources(const OpenGLShaderBase& new_shader);
};

//
// OpenGLRenderCommandBuffer
//

class OpenGLRenderCommandBuffer : public RenderCommandBuffer, public virtual OpenGLCommandBufferBase
{
  public:
    OpenGLRenderCommandBuffer() = default;
    ~OpenGLRenderCommandBuffer() override = default;

    void push_constant(std::string_view name, uint32_t size, const void* data) const override;

    void begin_render_pass(std::shared_ptr<RenderPass> render_pass) override;
    void end_render_pass() override;

    void bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline) override;
    void bind_pipeline(std::shared_ptr<ComputePipeline> pipeline) override;

    void draw(const VertexBuffer& vertex) const override;
    void draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const override;

    void draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const override;
    void draw_indexed_instanced(const VertexBuffer& vertex,
                                const IndexBuffer& index,
                                uint32_t instance_count) const override;

    void dispatch(glm::uvec3 group_count) const override;

    [[nodiscard]] std::shared_ptr<RenderPass> get_current_render_pass() const override
    {
        return std::dynamic_pointer_cast<RenderPass>(m_bound_render_pass);
    }

  private:
    std::shared_ptr<OpenGLRenderPass> m_bound_render_pass{};

    std::shared_ptr<OpenGLGraphicsPipeline> m_bound_graphics_pipeline{};
    std::shared_ptr<OpenGLComputePipeline> m_bound_compute_pipeline{};
};

//
// OpenGLComputeCommandBuffer
//

class OpenGLComputeCommandBuffer : public ComputeCommandBuffer, public virtual OpenGLCommandBufferBase
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
