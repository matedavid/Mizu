#include "opengl_command_buffer.h"

#include "renderer/abstraction/backend/opengl/opengl_buffers.h"
#include "renderer/abstraction/backend/opengl/opengl_graphics_pipeline.h"
#include "renderer/abstraction/backend/opengl/opengl_render_pass.h"
#include "renderer/abstraction/backend/opengl/opengl_resource_group.h"
#include "renderer/abstraction/backend/opengl/opengl_shader.h"

namespace Mizu::OpenGL {

//
// OpenGLCommandBufferBase
//

void OpenGLCommandBufferBase::end_base() {
    m_bound_resources.clear();
}

void OpenGLCommandBufferBase::bind_resource_group_base(const std::shared_ptr<ResourceGroup>& resource_group,
                                                       uint32_t set) {
    const auto native_rg = std::dynamic_pointer_cast<OpenGLResourceGroup>(resource_group);
    m_bound_resources.insert({set, native_rg});
}

//
// OpenGLRenderCommandBuffer
//

void OpenGLRenderCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                    uint32_t set) {
    bind_resource_group_base(resource_group, set);

    if (m_bound_pipeline != nullptr) {
        bind_bound_resources(m_bound_pipeline->get_shader());
    }
}

void OpenGLRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    m_bound_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    m_bound_pipeline->set_state();

    bind_bound_resources(m_bound_pipeline->get_shader());
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
    const auto native_vertex = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertex);
    native_vertex->bind();

    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(native_vertex->count()));
}

void OpenGLRenderCommandBuffer::draw_indexed(const std::shared_ptr<VertexBuffer>& vertex,
                                             const std::shared_ptr<IndexBuffer>& index) {
    const auto native_vertex = std::dynamic_pointer_cast<OpenGLVertexBuffer>(vertex);
    const auto native_index = std::dynamic_pointer_cast<OpenGLIndexBuffer>(index);

    native_vertex->bind();
    native_index->bind();

    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(index->count()), GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderCommandBuffer::bind_bound_resources(const std::shared_ptr<OpenGLShader>& shader) const {
    for (const auto& [set, resource] : m_bound_resources) {
        [[maybe_unused]] const bool ok = resource->bake(shader, set);
        // assert(ok && "Could not bake resource group");
    }
}

//
// OpenGLComputeCommandBuffer
//

} // namespace Mizu::OpenGL
