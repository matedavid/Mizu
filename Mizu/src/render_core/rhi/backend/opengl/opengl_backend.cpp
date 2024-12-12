#include "opengl_backend.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "utility/assert.h"
#include "utility/logging.h"

namespace Mizu::OpenGL
{

#if MIZU_DEBUG

// Callback function for OpenGL debugging
static void opengl_debug_callback(GLenum source,
                                  GLenum type,
                                  GLuint id,
                                  GLenum severity,
                                  GLsizei length,
                                  const GLchar* message,
                                  const void* userParam)
{
    // Ignore non-significant error/warning codes
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
        return;

    if (source == GL_DEBUG_SOURCE_APPLICATION)
        return;

    std::string source_str, type_str;

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        source_str = "API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        source_str = "Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        source_str = "Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        source_str = "Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        source_str = "Application";
        return;
    case GL_DEBUG_SOURCE_OTHER:
        source_str = "Other";
        break;
    default:
        source_str = "Unknown";
        break;
    }

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        type_str = "Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        type_str = "Deprecated Behavior";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        type_str = "Undefined Behavior";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        type_str = "Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        type_str = "Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        type_str = "Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        type_str = "Push Group";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        type_str = "Pop Group";
        break;
    case GL_DEBUG_TYPE_OTHER:
        type_str = "Other";
        break;
    default:
        type_str = "Unknown";
        break;
    }

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        MIZU_LOG_ERROR("[OpenGL {}] - {}: {}", type_str, source_str, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        MIZU_LOG_WARNING("[OpenGL {}] - {}: {}", type_str, source_str, message);
        break;
    case GL_DEBUG_SEVERITY_LOW:
        MIZU_LOG_INFO("[OpenGL {}] - {}: {}", type_str, source_str, message);
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        MIZU_LOG_INFO("[OpenGL {}] - {}: {}", type_str, source_str, message);
        break;
    default:
        MIZU_LOG_INFO("[OpenGL {}] - {}: {}", type_str, source_str, message);
        break;
    }
}

#endif

OpenGLBackend::~OpenGLBackend()
{
    if (m_offscreen_window != nullptr)
    {
        glfwDestroyWindow(m_offscreen_window);
        glfwTerminate();
    }
}

bool OpenGLBackend::initialize([[maybe_unused]] const RendererConfiguration& config)
{
    MIZU_ASSERT(std::holds_alternative<OpenGLSpecificConfiguration>(config.backend_specific_config),
                "backend_specific_configuration is not OpenGLSpecificConfiguration");

    const auto& cfg = std::get<OpenGLSpecificConfiguration>(config.backend_specific_config);

    // NOTE: If cfg.create_context is false, context should have been previosly created by Window

    if (cfg.create_context)
    {
        const int32_t result = glfwInit();
        MIZU_ASSERT(result, "Failed to initialize glfw");

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Windowless context

        m_offscreen_window = glfwCreateWindow(640, 480, "", nullptr, nullptr);
        glfwMakeContextCurrent(m_offscreen_window);
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        MIZU_LOG_ERROR("Failed to load OpenGL function pointers");
        return false;
    }

    glEnable(GL_FRAMEBUFFER_SRGB);

    MIZU_LOG_INFO("Selected device: {}, OpenGL version: {}",
                  reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
                  reinterpret_cast<const char*>(glGetString(GL_VERSION)));

#if MIZU_DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS); // Ensure messages are synchronous for debugging
    glDebugMessageCallback(opengl_debug_callback, nullptr);
#endif

    return true;
}

void OpenGLBackend::wait_idle() const
{
    glFinish();
}

} // namespace Mizu::OpenGL