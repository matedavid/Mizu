#pragma once

#include <glad/glad.h>
#include <string_view>

namespace Mizu::OpenGL
{

#if GL_KHR_debug

class OpenGLDebug
{
  public:
    static void begin_gpu_marker(const char* label);
    static void end_gpu_marker();

    static void set_debug_name(GLenum type, GLuint object, std::string_view name);
};

#define GL_DEBUG_BEGIN_GPU_MARKER(label) OpenGLDebug::begin_gpu_marker(label)
#define GL_DEBUG_END_GPU_MARKER() OpenGLDebug::end_gpu_marker()

#define GL_DEBUG_SET_OBJECT_NAME(type, object, name) OpenGLDebug::set_debug_name(type, object, name)

#else

#define GL_DEBUG_BEGIN_LABEL(label)
#define GL_DEBUG_END_LABEL()

#endif

} // namespace Mizu::OpenGL
