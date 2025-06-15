#include "opengl_command_buffer.h"

#include "render_core/resources/buffers.h"

#include "render_core/rhi/resource_view.h"

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

/*

//
// OpenGLCommandBufferBase
//

void OpenGLCommandBufferBase::end()
{
    m_resources.clear();
}

void OpenGLCommandBufferBase::bind_resource_group(std::shared_ptr<ResourceGroup> resource_group, uint32_t set)
{
    if (m_currently_bound_shader == nullptr)
    {
        CommandBufferResourceGroup rg{};
        rg.resource_group = std::dynamic_pointer_cast<OpenGLResourceGroup>(resource_group);
        rg.is_bound = false;

        m_resources.push_back(rg);
    }
    else
    {
        CommandBufferResourceGroup rg{};
        rg.resource_group = std::dynamic_pointer_cast<OpenGLResourceGroup>(resource_group);
        rg.is_bound = true;

        MIZU_ASSERT(rg.resource_group->bake(*m_currently_bound_shader, set), "Could not bake ResourceGroup");
        rg.resource_group->bind(*m_currently_bound_shader);

        m_resources.push_back(rg);
    }
}

void OpenGLCommandBufferBase::transition_resource([[maybe_unused]] const ImageResource& image,
                                                  [[maybe_unused]] ImageResourceState old_state,
                                                  [[maybe_unused]] ImageResourceState new_state) const
{
    // In OpenGL, image transitions are not needed
}

void OpenGLCommandBufferBase::transition_resource([[maybe_unused]] const ImageResource& image,
                                                  [[maybe_unused]] ImageResourceState old_state,
                                                  [[maybe_unused]] ImageResourceState new_state,
                                                  [[maybe_unused]] ImageResourceViewRange range) const
{
    // In OpenGL, image transitions are not needed
}

void OpenGLCommandBufferBase::copy_buffer_to_buffer(const BufferResource& source, const BufferResource& dest) const
{
    (void)source;
    (void)dest;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLCommandBufferBase::copy_buffer_to_image(const BufferResource& buffer, const ImageResource& image) const
{
    (void)buffer;
    (void)image;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLCommandBufferBase::build_blas(const BottomLevelAccelerationStructure& blas) const
{
    (void)blas;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLCommandBufferBase::begin_debug_label(const std::string_view& label) const
{
    GL_DEBUG_BEGIN_LABEL(label.data());
}

void OpenGLCommandBufferBase::end_debug_label() const
{
    GL_DEBUG_END_LABEL();
}

void OpenGLCommandBufferBase::bind_resources(const OpenGLShaderBase& new_shader)
{
    for (CommandBufferResourceGroup& rg : m_resources)
    {
        if (rg.resource_group->bake(new_shader, 0))
        {
            rg.resource_group->bind(new_shader);
            rg.is_bound = true;
        }
        else
        {
            rg.is_bound = false;
        }
    }

    auto it = m_resources.begin();
    while (it != m_resources.end())
    {
        if (!it->is_bound)
        {
            it = m_resources.erase(it);
        }
        else
        {
            ++it;
        }
    }
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

void OpenGLRenderCommandBuffer::begin_render_pass(std::shared_ptr<RenderPass> render_pass)
{
    MIZU_ASSERT(m_bound_render_pass == nullptr, "Can't bind RenderPass because a RenderPass has already been bound");

    m_bound_render_pass = std::dynamic_pointer_cast<OpenGLRenderPass>(render_pass);
    m_bound_render_pass->begin();
}

void OpenGLRenderCommandBuffer::end_render_pass()
{
    MIZU_ASSERT(m_bound_render_pass != nullptr, "Can't end RenderPass because no RenderPass is bound");

    m_bound_render_pass->end();
    m_bound_render_pass = nullptr;

    m_bound_graphics_pipeline = nullptr;
    m_bound_compute_pipeline = nullptr;
    m_currently_bound_shader = nullptr;
}

void OpenGLRenderCommandBuffer::bind_pipeline(std::shared_ptr<GraphicsPipeline> pipeline)
{
    MIZU_ASSERT(m_bound_render_pass != nullptr, "Can't bind pipeline because no RenderPass is bound");

    m_bound_graphics_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    m_bound_compute_pipeline = nullptr;

    m_bound_graphics_pipeline->set_state();

    bind_resources(*m_bound_graphics_pipeline->get_shader());
    m_currently_bound_shader = m_bound_graphics_pipeline->get_shader();
}

void OpenGLRenderCommandBuffer::bind_pipeline(std::shared_ptr<ComputePipeline> pipeline)
{
    MIZU_ASSERT(m_bound_render_pass != nullptr, "Can't bind pipeline because no RenderPass is bound");

    m_bound_graphics_pipeline = nullptr;
    m_bound_compute_pipeline = std::dynamic_pointer_cast<OpenGLComputePipeline>(pipeline);

    m_bound_compute_pipeline->set_state();

    bind_resources(*m_bound_compute_pipeline->get_shader());
    m_currently_bound_shader = m_bound_compute_pipeline->get_shader();
}

void OpenGLRenderCommandBuffer::bind_pipeline(std::shared_ptr<RayTracingPipeline> pipeline)
{
    (void)pipeline;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLRenderCommandBuffer::draw(const VertexBuffer& vertex) const
{
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr, "Can't draw because no GraphicsPipeline has been bound");

    const auto& native_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(vertex.get_resource());
    glBindBuffer(GL_ARRAY_BUFFER, native_buffer->handle());

    m_bound_graphics_pipeline->set_vertex_buffer_layout();

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertex.get_count()));
}

void OpenGLRenderCommandBuffer::draw_indexed(const VertexBuffer& vertex, const IndexBuffer& index) const
{
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr, "Can't draw indexed because no GraphicsPipeline has been bound");

    const auto& vertex_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(vertex.get_resource());
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer->handle());

    m_bound_graphics_pipeline->set_vertex_buffer_layout();

    const auto& index_buffer = std::dynamic_pointer_cast<OpenGLBufferResource>(index.get_resource());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer->handle());

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index.get_count()), GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderCommandBuffer::draw_instanced(const VertexBuffer& vertex, uint32_t instance_count) const
{
    (void)vertex;
    (void)instance_count;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLRenderCommandBuffer::draw_indexed_instanced(const VertexBuffer& vertex,
                                                       const IndexBuffer& index,
                                                       uint32_t instance_count) const
{
    (void)vertex;
    (void)index;
    (void)instance_count;

    MIZU_UNREACHABLE("Unimplemented");
}

void OpenGLRenderCommandBuffer::dispatch(glm::uvec3 group_count) const
{
    MIZU_ASSERT(m_bound_compute_pipeline != nullptr,
                "To call dispatch on RenderCommandBuffer you must have previously bound a ComputePipeline");

    glDispatchCompute(group_count.x, group_count.y, group_count.z);
}

void OpenGLRenderCommandBuffer::trace_rays(glm::uvec3 dimensions) const
{
    (void)dimensions;
    MIZU_UNREACHABLE("Unimplemented");
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

*/

} // namespace Mizu::OpenGL
