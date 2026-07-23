#include "render/state_manager/render_settings_layer_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

RenderSettingsLayerStateManager* g_render_settings_layer_state_manager;

template class MIZU_RENDER_API BaseStateManager<
    RenderSettingsLayerStaticState,
    RenderSettingsLayerDynamicState,
    RenderSettingsLayerHandle,
    RenderSettingsLayerConfig>;

} // namespace Mizu