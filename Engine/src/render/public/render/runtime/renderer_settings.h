#pragma once

#include "core/settings_manager/settings_manager.h"
#include "render_core/rhi/device.h"

namespace Mizu
{

#define MIZU_RENDERER_SETTINGS_MEMBERS(X)             \
    X(GraphicsApi, graphics_api, GraphicsApi::Vulkan) \
    X(bool, validations_enabled, true)                \
    X(uint32_t, frames_in_flight, 2)                  \
    X(bool, gpu_driven_rendering_enabled, true)

MIZU_CREATE_SETTING(RendererSettings, MIZU_RENDERER_SETTINGS_MEMBERS);

#undef MIZU_RENDERER_SETTINGS_MEMBERS

} // namespace Mizu