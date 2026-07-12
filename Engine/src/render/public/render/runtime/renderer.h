#pragma once

#include "render_core/rhi/device.h"

#include "mizu_render_module.h"

namespace Mizu
{

class GameRenderer;

MIZU_RENDER_API extern Device* g_render_device;
MIZU_RENDER_API extern GameRenderer* g_game_renderer;

} // namespace Mizu