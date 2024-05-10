#include "opengl_command_buffer.h"

#include "backend/opengl/opengl_buffers.h"
#include "backend/opengl/opengl_graphics_pipeline.h"
#include "backend/opengl/opengl_render_pass.h"

namespace Mizu::OpenGL {

//
// OpenGLRenderCommandBuffer
//

void OpenGLRenderCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                    uint32_t set) {
    // TODO:
}

void OpenGLRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    const auto native_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    native_pipeline->set_state();
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

//
// OpenGLComputeCommandBuffer
//

void OpenGLComputeCommandBuffer::bind_resource_group(const std::shared_ptr<ResourceGroup>& resource_group,
                                                     uint32_t set) {
    // TODO:
}

} // namespace Mizu::OpenGL
