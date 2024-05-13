#include "opengl_backend.h"

#include <glad/glad.h>

#include "utility/logging.h"

namespace Mizu::OpenGL {

bool OpenGLBackend::initialize([[maybe_unused]] const RendererConfiguration& config) {
    assert(std::holds_alternative<OpenGLSpecificConfiguration>(config.backend_specific_config)
           && "backend_specific_configuration is not OpenGLSpecificConfiguration");

    // NOTE: OpenGL context should have been created by Window if OpenGL backend is requested

    if (!gladLoadGL()) {
        MIZU_LOG_ERROR("Failed to load OpenGL function pointers");
        return false;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    MIZU_LOG_INFO("Selected device: {}", (const char*)renderer);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    MIZU_LOG_INFO("OpenGL version: {}", version);

    return true;
}

} // namespace Mizu::OpenGL