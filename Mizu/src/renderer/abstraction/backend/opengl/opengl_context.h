#pragma once

#include <glad/glad.h>

namespace Mizu::OpenGL {

#if GL_KHR_debug

void begin_debug_label(const char* label);
void end_debug_label();

#define GL_DEBUG_BEGIN_LABEL(label) begin_debug_label(label)
#define GL_DEBUG_ENDL_LABEL() end_debug_label()

#else

#define GL_DEBUG_BEGIN_LABEL(label)
#define GL_DEBUG_ENDL_LABEL()

#endif

} // namespace Mizu::OpenGL
