#include "opengl_command_buffer.h"

#include "backend/opengl/opengl_graphics_pipeline.h"
#include "backend/opengl/opengl_render_pass.h"

namespace Mizu::OpenGL {

//
// OpenGLRenderCommandBuffer
//

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

} // namespace Mizu::OpenGL
