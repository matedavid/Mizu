#include "opengl_compute_pipeline.h"

#include "renderer/abstraction/backend/opengl/opengl_shader.h"

namespace Mizu::OpenGL {

OpenGLComputePipeline::OpenGLComputePipeline(const Description& desc) {
    // TODO:
}

void OpenGLComputePipeline::push_constant(std::string_view name, uint32_t size, const void* data) {
    // TODO:
}

} // namespace Mizu::OpenGL