#pragma once

#include <algorithm>
#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"
#include "state_manager/base_state_manager2.h"

#include "mizu_render_module.h"
#include "render/render_graph_renderer_settings.h"

namespace Mizu
{

struct RendererSettingsStaticState
{
};

struct RendererSettingsDynamicState
{
    RenderGraphRendererSettings settings;

    bool has_changed(const RendererSettingsDynamicState& other) const
    {
        return cascaded_shadows_settings_has_changed(other.settings.cascaded_shadows)
               || debug_settings_has_changed(other.settings.debug);
    }

  private:
    bool cascaded_shadows_settings_has_changed(const CascadedShadowsSettings& other) const
    {
        const bool resolution_has_changed = settings.cascaded_shadows.resolution != other.resolution;
        const bool num_cascades_has_changed = settings.cascaded_shadows.num_cascades != other.num_cascades;
        const bool cascade_split_factors_have_changed = !std::equal(
            settings.cascaded_shadows.cascade_split_factors.begin(),
            settings.cascaded_shadows.cascade_split_factors.end(),
            other.cascade_split_factors.begin());

        return resolution_has_changed || num_cascades_has_changed || cascade_split_factors_have_changed;
    }

    bool debug_settings_has_changed(const DebugSettings& other) const
    {
        const bool debug_view_has_changed = settings.debug.view != other.view;

        return debug_view_has_changed;
    }
};

struct RendererSettingsConfig : BaseStateManagerConfig
{
    static constexpr uint32_t MaxNumHandles = 2;
    static constexpr bool UpdateDynamicStateOnBeginTick = false;
};

MIZU_STATE_MANAGER_CREATE_HANDLE(RendererSettingsHandle);

using RendererSettingsStateManager = BaseStateManager<
    RendererSettingsStaticState,
    RendererSettingsDynamicState,
    RendererSettingsHandle,
    RendererSettingsConfig>;

MIZU_RENDER_API extern RendererSettingsStateManager* g_renderer_settings_state_manager;

struct RendererSettingsConfig2 : BaseStateManagerConfig2
{
    static constexpr uint64_t MaxNumHandles = 2;
    static constexpr bool Interpolate = false;

    static constexpr std::string_view Identifier = "RendererSettingsStateManager";
};

using RendererSettingsStateManager2 = BaseStateManager2<
    RendererSettingsStaticState,
    RendererSettingsDynamicState,
    RendererSettingsHandle,
    RendererSettingsConfig2>;

MIZU_RENDER_API extern RendererSettingsStateManager2* g_renderer_settings_state_manager2;

MIZU_RENDER_API void sim_update_renderer_settings(const RendererSettingsDynamicState& ds);
MIZU_RENDER_API RendererSettingsDynamicState& sim_edit_renderer_settings();
MIZU_RENDER_API const RendererSettingsDynamicState& rend_get_renderer_settings();

} // namespace Mizu