#include "opengl_context.h"

namespace Mizu::OpenGL {

static uint32_t s_current_debug_idx = 0;

void begin_debug_label(const char* label) {
    glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, s_current_debug_idx++, -1, label);
}

void end_debug_label() {
    glPopDebugGroupKHR();
    s_current_debug_idx--;
}

} // namespace Mizu::OpenGL
