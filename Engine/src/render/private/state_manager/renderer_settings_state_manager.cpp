#include "render/state_manager/renderer_settings_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

RendererSettingsStateManager* g_renderer_settings_state_manager;

template class MIZU_RENDER_API BaseStateManager<
    RendererSettingsStaticState,
    RendererSettingsDynamicState,
    RendererSettingsHandle,
    RendererSettingsConfig>;

static RendererSettingsHandle s_renderer_settings_handle;

void sim_update_renderer_settings(const RendererSettingsDynamicState& ds)
{
    sim_edit_renderer_settings() = ds;
}

RendererSettingsDynamicState& sim_edit_renderer_settings()
{
    if (!s_renderer_settings_handle.is_valid())
    {
        s_renderer_settings_handle = g_renderer_settings_state_manager->sim_create(
            RendererSettingsStaticState{}, RendererSettingsDynamicState{});
    }

    return g_renderer_settings_state_manager->sim_edit(s_renderer_settings_handle);
}

const RendererSettingsDynamicState& rend_get_renderer_settings()
{
    if (!s_renderer_settings_handle.is_valid())
    {
        static RendererSettingsDynamicState s_default_renderer_settings;
        return s_default_renderer_settings;
    }

    return g_renderer_settings_state_manager->rend_get_dynamic_state(s_renderer_settings_handle);
}

} // namespace Mizu