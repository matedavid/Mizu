#include "opengl_context.h"

#include <cstdint>

namespace Mizu::OpenGL
{

static uint32_t s_current_debug_idx = 0;

void OpenGLDebug::begin_gpu_marker(const char* label)
{
    glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, s_current_debug_idx++, -1, label);
}

void OpenGLDebug::end_gpu_marker()
{
    glPopDebugGroupKHR();
    s_current_debug_idx--;
}

void OpenGLDebug::set_debug_name(GLenum type, GLuint object, std::string_view name)
{
    glObjectLabel(type, object, static_cast<GLsizei>(name.size()), name.data());
}

} // namespace Mizu::OpenGL
