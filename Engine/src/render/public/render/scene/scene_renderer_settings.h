#pragma once

#include "core/settings_manager/settings_manager.h"

namespace Mizu
{

#define MIZU_SCENE_RENDERER_SETTINGS_MEMBERS(X) X(bool, depth_prepass_enabled, true)

MIZU_CREATE_SETTING(SceneRendererSettings, MIZU_SCENE_RENDERER_SETTINGS_MEMBERS);

#undef MIZU_SCENE_RENDERER_SETTINGS_MEMBERS

} // namespace Mizu