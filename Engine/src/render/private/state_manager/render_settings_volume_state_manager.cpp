#include "render/state_manager/render_settings_volume_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

RenderSettingsVolumeStateManager* g_render_settings_volume_state_manager;

template class MIZU_RENDER_API BaseStateManager<
    RenderSettingsVolumeStaticState,
    RenderSettingsVolumeDynamicState,
    RenderSettingsVolumeHandle,
    RenderSettingsVolumeConfig>;

} // namespace Mizu