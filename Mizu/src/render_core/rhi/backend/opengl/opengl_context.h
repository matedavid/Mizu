#pragma once

#include <glad/glad.h>

namespace Mizu::OpenGL
{

#if GL_KHR_debug

class OpenGLDebug
{
  public:
    static void begin_debug_label(const char* label);
    static void end_debug_label();
};

#define GL_DEBUG_BEGIN_LABEL(label) OpenGLDebug::begin_debug_label(label)
#define GL_DEBUG_END_LABEL() OpenGLDebug::end_debug_label()

#else

#define GL_DEBUG_BEGIN_LABEL(label)
#define GL_DEBUG_END_LABEL()

#endif

} // namespace Mizu::OpenGL
