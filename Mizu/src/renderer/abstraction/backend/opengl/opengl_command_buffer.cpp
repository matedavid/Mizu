#include "opengl_command_buffer.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_compute_pipeline.h"
#include "renderer/abstraction/backend/opengl/opengl_context.h"
#include "renderer/abstraction/backend/opengl/opengl_graphics_pipeline.h"
#include "renderer/abstraction/backend/opengl/opengl_render_pass.h"
#include "renderer/abstraction/backend/opengl/opengl_resource_group.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"

#include "utility/assert.h"

namespace Mizu::OpenGL {

//
// OpenGLCommandBufferBase
//

void OpenGLCommandBufferBase::end() {
    m_bound_resources.clear();
}

void OpenGLCommandBufferBase::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                  [[maybe_unused]] uint32_t set) {
    const auto native_rg = std::dynamic_pointer_cast<OpenGLResourceGroup>(resource_group);
    m_bound_resources.push_back(native_rg);
}

void OpenGLCommandBufferBase::bind_bound_resources(const std::shared_ptr<OpenGLShaderBase>& shader) const {
    for (const auto& resource : m_bound_resources) {
        resource->bind(shader);
    }
}

void OpenGLCommandBufferBase::transition_resource([[maybe_unused]] const std::shared_ptr<Texture2D>& texture,
                                                  [[maybe_unused]] ImageResourceState old_state,
                                                  [[maybe_unused]] ImageResourceState new_state) const {
    // In OpenGL, image transitions are not needed
}

void OpenGLCommandBufferBase::begin_debug_label(const std::string& label) const {
    GL_DEBUG_BEGIN_LABEL(label.c_str());
}

void OpenGLCommandBufferBase::end_debug_label() const {
    GL_DEBUG_END_LABEL();
}

//
// OpenGLRenderCommandBuffer
//

void OpenGLRenderCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                    uint32_t set) {
    OpenGLCommandBufferBase::bind_resource_group(resource_group, set);

    if (m_bound_graphics_pipeline != nullptr) {
        bind_bound_resources(m_bound_graphics_pipeline->get_shader());
    } else if (m_bound_compute_pipeline != nullptr) {
        bind_bound_resources(m_bound_compute_pipeline->get_shader());
    }
}

void OpenGLRenderCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) {
    if (m_bound_graphics_pipeline != nullptr) {
        m_bound_graphics_pipeline->push_constant(name, size, data);
    } else if (m_bound_compute_pipeline != nullptr) {
        m_bound_compute_pipeline->push_constant(name, size, data);
    } else {
        MIZU_LOG_WARNING("Can't push constant because no GraphicsPipeline has been bound");
    }
}

void OpenGLRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    m_bound_resources.clear();

    m_bound_graphics_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    m_bound_graphics_pipeline->set_state();

    // bind_bound_resources(m_bound_graphics_pipeline->get_shader());

    m_bound_compute_pipeline = nullptr;
}

void OpenGLRenderCommandBuffer::bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) {
    m_bound_resources.clear();

    m_bound_compute_pipeline = std::dynamic_pointer_cast<OpenGLComputePipeline>(pipeline);
    m_bound_compute_pipeline->set_state();

    // bind_bound_resources(m_bound_compute_pipeline->get_shader());

    m_bound_graphics_pipeline = nullptr;
}

void OpenGLRenderCommandBuffer::begin_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<OpenGLRenderPass>(render_pass);
    native_render_pass->begin();
}

void OpenGLRenderCommandBuffer::end_render_pass(const std::shared_ptr<RenderPass>& render_pass) {
    const auto native_render_pass = std::dynamic_pointer_cast<OpenGLRenderPass>(render_pass);
    native_render_pass->end();
}

void OpenGLRenderCommandBuffer::draw(const std::shared_ptr<VertexBuffer>& vertex) {
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr,
                "To call draw on RenderCommandBuffer you must have previously bound a GraphicsPipeline");

    const auto native_vertex = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertex);
    native_vertex->bind();

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(native_vertex->count()));
}

void OpenGLRenderCommandBuffer::draw_indexed(const std::shared_ptr<VertexBuffer>& vertex,
                                             const std::shared_ptr<IndexBuffer>& index) {
    MIZU_ASSERT(m_bound_graphics_pipeline != nullptr,
                "To call draw_indexed on RenderCommandBuffer you must have previously bound a GraphicsPipeline");

    const auto native_vertex = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertex);
    const auto native_index = std::dynamic_pointer_cast<OpenGLIndexBuffer>(index);

    native_vertex->bind();
    native_index->bind();

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index->count()), GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderCommandBuffer::dispatch(glm::uvec3 group_count) {
    MIZU_ASSERT(m_bound_compute_pipeline != nullptr,
                "To call dispatch on RenderCommandBuffer you must have previously bound a ComputePipeline");

    glDispatchCompute(group_count.x, group_count.y, group_count.z);
}

//
// OpenGLComputeCommandBuffer
//

void OpenGLComputeCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                     uint32_t set) {
    OpenGLCommandBufferBase::bind_resource_group(resource_group, set);

    if (m_bound_pipeline != nullptr) {
        bind_bound_resources(m_bound_pipeline->get_shader());
    }
}

void OpenGLComputeCommandBuffer::push_constant(std::string_view name, uint32_t size, const void* data) {
    if (m_bound_pipeline == nullptr) {
        MIZU_LOG_WARNING("Can't push constant because no ComputePipeline has been bound");
        return;
    }

    m_bound_pipeline->push_constant(name, size, data);
}

void OpenGLComputeCommandBuffer::bind_pipeline(const std::shared_ptr<ComputePipeline>& pipeline) {
    m_bound_pipeline = std::dynamic_pointer_cast<OpenGLComputePipeline>(pipeline);
    m_bound_pipeline->set_state();

    bind_bound_resources(m_bound_pipeline->get_shader());
}

void OpenGLComputeCommandBuffer::dispatch(glm::uvec3 group_count) {
    MIZU_ASSERT(m_bound_pipeline != nullptr, "To call dispatch you must have previously bound a ComputePipeline");

    glDispatchCompute(group_count.x, group_count.y, group_count.z);
}

} // namespace Mizu::OpenGL
