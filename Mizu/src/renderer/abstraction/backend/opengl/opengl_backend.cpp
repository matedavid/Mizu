#include "opengl_backend.h"

#include <cassert>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "utility/logging.h"

namespace Mizu::OpenGL {

static void opengl_error_callback(GLenum source,
                                  GLenum type,
                                  GLuint id,
                                  GLenum severity,
                                  GLsizei length,
                                  const GLchar* message,
                                  const void* userParam) {
    if (type == GL_DEBUG_TYPE_ERROR) {
        MIZU_LOG_ERROR("GL ERROR: type = {:0x}, severity = {:0x}, message = {}", type, severity, message);
    } else {
        MIZU_LOG_WARNING("GL WARNING: type = {:0x}, severity = {:0x}, message = {}", type, severity, message);
    }
}

OpenGLBackend::~OpenGLBackend() {
    if (m_offscreen_window != nullptr) {
        glfwDestroyWindow(m_offscreen_window);
        glfwTerminate();
    }
}

bool OpenGLBackend::initialize([[maybe_unused]] const RendererConfiguration& config) {
    assert(std::holds_alternative<OpenGLSpecificConfiguration>(config.backend_specific_config)
           && "backend_specific_configuration is not OpenGLSpecificConfiguration");

    const auto cfg = std::get<OpenGLSpecificConfiguration>(config.backend_specific_config);

    // NOTE: If cfg.create_context is false, context should have been previosly created by Window

    if (cfg.create_context) {
        const int result = glfwInit();
        assert(result && "Failed to initialize glfw");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Windowless context

        m_offscreen_window = glfwCreateWindow(640, 480, "", nullptr, nullptr);
        glfwMakeContextCurrent(m_offscreen_window);
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        MIZU_LOG_ERROR("Failed to load OpenGL function pointers");
        return false;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    MIZU_LOG_INFO("Selected device: {}", (const char*)renderer);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    MIZU_LOG_INFO("OpenGL version: {}", version);

    // Enable debugging
    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(opengl_error_callback, nullptr);

    return true;
}

} // namespace Mizu::OpenGL