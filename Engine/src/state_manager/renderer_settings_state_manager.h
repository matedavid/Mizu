#pragma once

#include <glm/glm.hpp>

#include "renderer/render_graph_renderer_settings.h"
#include "state_manager/base_state_manager.h"

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

extern RendererSettingsStateManager* g_renderer_settings_state_manager;

void sim_update_renderer_settings(const RendererSettingsDynamicState& ds);
RendererSettingsDynamicState& sim_edit_renderer_settings();
const RendererSettingsDynamicState& rend_get_renderer_settings();

} // namespace Mizu