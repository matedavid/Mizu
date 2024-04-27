#include "opengl_command_buffer.h"

namespace Mizu::OpenGL {

void OpenGLCommandBuffer::begin() const {}

void OpenGLCommandBuffer::end() const {}

void OpenGLCommandBuffer::submit() const {}

void OpenGLCommandBuffer::submit([[maybe_unused]] const CommandBufferSubmitInfo& info) const {}

} // namespace Mizu::OpenGL
