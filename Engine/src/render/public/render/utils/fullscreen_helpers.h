#pragma once

#include <memory>

#include "render_core/rhi/command_buffer.h"

#include "mizu_render_module.h"

namespace Mizu
{

class BufferResource;

namespace FullscreenHelpers
{

MIZU_RENDER_API bool init();
MIZU_RENDER_API void shutdown();

MIZU_RENDER_API void draw_fullscreen_triangle(CommandBuffer& command);
MIZU_RENDER_API void draw_fullscreen_quad(CommandBuffer& command);

} // namespace FullscreenHelpers

} // namespace Mizu
