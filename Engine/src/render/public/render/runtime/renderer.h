#pragma once

#include "render_core/rhi/device.h"

#include "mizu_render_module.h"
#include "render/runtime/game_renderer.h"

namespace Mizu
{

MIZU_RENDER_API extern Device* g_render_device;
MIZU_RENDER_API extern GameRenderer* g_game_renderer;

} // namespace Mizu