#include "opengl_backend.h"

#include <cassert>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "utility/logging.h"

namespace Mizu::OpenGL {

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

    MIZU_LOG_INFO("Selected device: {}, OpenGL version: {}",
                  reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                  reinterpret_cast<const char*>(glGetString(GL_VERSION)));

    return true;
}

void OpenGLBackend::wait_idle() const {
    glFinish();
}

} // namespace Mizu::OpenGL