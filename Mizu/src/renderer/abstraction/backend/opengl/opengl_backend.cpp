#include "opengl_backend.h"

#include <glad/glad.h>

#include "utility/logging.h"

namespace Mizu::OpenGL {

OpenGLBackend::~OpenGLBackend() {
    if (m_display != nullptr && m_context != nullptr)
        eglDestroyContext(m_display, m_context);

    if (m_display != nullptr)
        eglTerminate(m_display);
}

bool OpenGLBackend::initialize([[maybe_unused]] const RendererConfiguration& config) {
    assert(std::holds_alternative<OpenGLSpecificConfiguration>(config.backend_specific_config)
           && "backend_specific_configuration is not OpenGLSpecificConfiguration");

    const auto cfg = std::get<OpenGLSpecificConfiguration>(config.backend_specific_config);

    if (cfg.create_context) {
        // clang-format off
        constexpr EGLint config_attribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,

            EGL_NONE,
        };
        // clang-format on

        m_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

        if (eglInitialize(m_display, nullptr, nullptr) != EGL_TRUE) {
            switch (eglGetError()) {
            case EGL_BAD_DISPLAY:
                MIZU_LOG_ERROR("Failed to initialize EGL Display: EGL_BAD_DISPLAY");
                break;
            case EGL_NOT_INITIALIZED:
                MIZU_LOG_ERROR("Failed to initialize EGL Display: EGL_NOT_INITIALIZED");
                break;
            default:
                MIZU_LOG_ERROR("Failed to initialize EGL Display: unknown error");
                break;
            }

            return false;
        }

        EGLint num_configs;
        EGLConfig egl_config;
        if (eglChooseConfig(m_display, config_attribs, &egl_config, 1, &num_configs) != EGL_TRUE) {
            switch (eglGetError()) {
            case EGL_BAD_DISPLAY:
                MIZU_LOG_ERROR("Failed to configure EGL Display: EGL_BAD_DISPLAY");
                break;
            case EGL_BAD_ATTRIBUTE:
                MIZU_LOG_ERROR("Failed to configure EGL Display: EGL_BAD_ATTRIBUTE");
                break;
            case EGL_NOT_INITIALIZED:
                MIZU_LOG_ERROR("Failed to configure EGL Display: EGL_NOT_INITIALIZED");
                break;
            case EGL_BAD_PARAMETER:
                MIZU_LOG_ERROR("Failed to configure EGL Display: EGL_BAD_PARAMETER");
                break;
            default:
                MIZU_LOG_ERROR("Failed to configure EGL Display: unknown error");
                break;
            }

            return false;
        }

        if (eglBindAPI(EGL_OPENGL_API) != EGL_TRUE) {
            MIZU_LOG_ERROR("Failed to bind OpenGL api");
            return false;
        }

        // clang-format off
        const EGLint attr[] = {
            EGL_CONTEXT_MAJOR_VERSION, 4,
            EGL_CONTEXT_MINOR_VERSION, 6,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        #ifndef NDEBUG
            EGL_CONTEXT_OPENGL_DEBUG,
            EGL_TRUE,
        #endif

            EGL_NONE,
        };
        // clang-format on

        m_context = eglCreateContext(m_display, egl_config, EGL_NO_CONTEXT, attr);
        if (eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context) != EGL_TRUE) {
            MIZU_LOG_ERROR("Failed to make OpenGL context current");
            return false;
        }
    }

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