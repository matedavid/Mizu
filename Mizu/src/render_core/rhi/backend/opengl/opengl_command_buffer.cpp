#include "opengl_command_buffer.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/backend/opengl/opengl_buffer_resource.h"
#include "render_core/rhi/backend/opengl/opengl_compute_pipeline.h"
#include "render_core/rhi/backend/opengl/opengl_context.h"
#include "render_core/rhi/backend/opengl/opengl_graphics_pipeline.h"
#include "render_core/rhi/backend/opengl/opengl_image_resource.h"
#include "render_core/rhi/backend/opengl/opengl_render_pass.h"
#include "render_core/rhi/backend/opengl/opengl_resource_group.h"
#include "render_core/rhi/backend/opengl/opengl_shader.h"

#include "utility/assert.h"

namespace Mizu::OpenGL
{

//
// OpenGLCommandBufferBase
//

void OpenGLCommandBufferBase::bind_resource_group(const ResourceGroup& resource_group,
                                                  [[maybe_unused]] uint32_t set) const
{
    MIZU_ASSERT(m_currently_bound_shader != nullptr, "Can't bind resource group because no pipeline has been bound");

    const OpenGLResourceGroup& native_resource_group = dynamic_cast<const OpenGLResourceGroup&>(resource_group);
    native_resource_group.bind(m_currently_bound_shader);
}

void OpenGLCommandBufferBase::transition_resource([[maybe_unused]] ImageResource& image,
                                                  [[maybe_unused]] ImageResourceState old_state,
                                                  [[maybe_unused]] ImageResourceState new_state) const
{
    // In OpenGL, image transitions are not needed
}

void OpenGLCommandBufferBase::begin_debug_label(const std::string_view& label) const
{
    GL_DEBUG_BEGIN_LABEL(label.data());
}

void OpenGLCommandBufferBase::end_debug_label() const
{
    GL_DEBUG_END_LABEL();
}

//
// OpenGLRenderCommandBuffer
//

void OpenGLRenderCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    if (m_bound_graphics_pipeline != nullptr)
    {
        m_bound_graphics_pipeline->push_constant(name, size, data);
    }
    else if (m_bound_compute_pipeline != nullptr)
    {
        m_bound_compute_pipeline->push_constant(name, size, data);
    }
    else
    {
        MIZU_UNREACHABLE("Can't push constant because no pipeline has been bound");
    }
}

void OpenGLRenderCommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    m_bound_graphics_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    m_bound_compute_pipeline = nullptr;

    m_bound_graphics_pipeline->set_state();

    m_currently_bound_shader = m_bound_graphics_pipeline->get_shader();
}

void OpenGLRenderCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    m_bound_graphics_pipeline = nullptr;
    m_bound_compute_pipeline = std::dynamic_pointer_cast<OpenGLComputePipeline>(pipeline);

    m_bound_compute_pipeline->set_state();

    m_currently_bound_shader = m_bound_compute_pipeline->get_shader();
}

void OpenGLRenderCommandBuffer::begin_render_pass(const RenderPass& render_pass) const
{
    const OpenGLRenderPass& native_render_pass = dynamic_cast<const OpenGLRenderPass&>(render_pass);
    native_render_pass.begin();
}

void OpenGLRenderCommandBuffer::end_render_pass(const RenderPass& render_pass) const
{
    const OpenGLRenderPass& native_render_pass = dynamic_cast<const OpenGLRenderPass&>(render_pass);
    native_render_pass.end();
}

void OpenGLRenderCommandBuffer::draw(const VertexBuffer& vertex) const
{
    const auto& native_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(vertex.get_resource());
    glBindBuffer(GL_ARRAY_BUFFER, native_buffer->handle());

    m_bound_graphics_pipeline->set_vertex_buffer_layout();

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex.get_count()));
}

void OpenGLRenderCommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    const auto& vertex_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(vertex.get_resource());
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->handle());

    m_bound_graphics_pipeline->set_vertex_buffer_layout();

    const auto& index_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(index.get_resource());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->handle());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index.get_count()), GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(m_bound_compute_pipeline != nullptr,
                "To call dispatch on RenderCommandBuffer you must have previously bound a ComputePipeline");

    glDispatchCompute(group_count.x, group_count.y, group_count.z);
}

//
// OpenGLComputeCommandBuffer
//

void OpenGLComputeCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr, "To push constant because no pipeline has been bound");

    m_bound_pipeline->push_constant(name, size, data);
}

void OpenGLComputeCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    m_bound_pipeline = std::dynamic_pointer_cast<OpenGLComputePipeline>(pipeline);
    m_bound_pipeline->set_state();

    m_currently_bound_shader = m_bound_pipeline->get_shader();
}

void OpenGLComputeCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(m_bound_pipeline != nullptr, "To call dispatch you must have previously bound a ComputePipeline");

    glDispatchCompute(group_count.x, group_count.y, group_count.z);
}

} // namespace Mizu::OpenGL
