#pragma once

#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"

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

MIZU_RENDER_API void sim_update_renderer_settings(const RendererSettingsDynamicState& ds);
MIZU_RENDER_API RendererSettingsDynamicState& sim_edit_renderer_settings();
MIZU_RENDER_API const RendererSettingsDynamicState& rend_get_renderer_settings();

} // namespace Mizu