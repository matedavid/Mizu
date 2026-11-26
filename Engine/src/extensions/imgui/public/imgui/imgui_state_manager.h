#pragma once

#include <cstdint>

#include "state_manager/base_state_manager.h"
#include "state_manager/state_manager_consumer.h"

#include "imgui/imgui_types.h"
#include "mizu_imgui_module.h"

namespace Mizu
{

struct ImGuiStaticState
{
};

struct ImGuiDynamicState
{
    const ImGuiFrameSnapshot* frame = nullptr;

    bool has_changed(const ImGuiDynamicState& other) const { return frame != other.frame; }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(ImGuiHandle);

struct ImGuiConfig : BaseStateManagerConfig
{
    static constexpr uint64_t MaxNumHandles = 1;
    static constexpr std::string_view Identifier = "ImGuiStateManager";
};

using ImGuiStateManager = BaseStateManager<ImGuiStaticState, ImGuiDynamicState, ImGuiHandle, ImGuiConfig>;
using ImGuiStateManagerConsumer = IStateManagerConsumer<ImGuiStateManager>;

MIZU_IMGUI_API extern ImGuiStateManager* g_imgui_state_manager;

} // namespace Mizu
