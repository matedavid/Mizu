/*
#include "render/state_manager/imgui_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

ImGuiStateManager* g_imgui_state_manager;

template class BaseStateManager<ImGuiStaticState, ImGuiDynamicState, ImGuiHandle, ImGuiConfig>;

static ImGuiHandle s_imgui_handle;

void sim_set_imgui_state(const ImGuiDynamicState& state)
{
    if (!s_imgui_handle.is_valid())
    {
        s_imgui_handle = g_imgui_state_manager->sim_create_handle({}, {});
    }

    g_imgui_state_manager->sim_update(s_imgui_handle, state);
}

void rend_execute_imgui_function()
{
    const ImGuiDynamicState& state = g_imgui_state_manager->rend_get_dynamic_state(s_imgui_handle);
    state.func();
}

} // namespace Mizu
*/