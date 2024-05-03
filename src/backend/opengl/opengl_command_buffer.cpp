#include "opengl_command_buffer.h"

#include "backend/opengl/opengl_graphics_pipeline.h"

namespace Mizu::OpenGL {

//
// OpenGLRenderCommandBuffer
//

void OpenGLRenderCommandBuffer::bind_pipeline(const std::shared_ptr<GraphicsPipeline>& pipeline) {
    const auto native_pipeline = std::dynamic_pointer_cast<OpenGLGraphicsPipeline>(pipeline);
    native_pipeline->set_state();
}

} // namespace Mizu::OpenGL
