#pragma once

#include <functional>

#include "state_manager/base_state_manager.h"

namespace Mizu
{

struct ImGuiStaticState
{
};

struct ImGuiDynamicState
{
    std::function<void()> func;
};

struct ImGuiConfig : BaseStateManagerConfig
{
    static constexpr uint32_t MaxNumHandles = 2;
};

MIZU_STATE_MANAGER_CREATE_HANDLE(ImGuiHandle);

using ImGuiStateManager = BaseStateManager<ImGuiStaticState, ImGuiDynamicState, ImGuiHandle, ImGuiConfig>;

extern ImGuiStateManager* g_imgui_state_manager;

void sim_set_imgui_state(const ImGuiDynamicState& state);
void rend_execute_imgui_function();

} // namespace Mizu