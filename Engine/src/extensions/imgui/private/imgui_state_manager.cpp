#include "imgui/imgui_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

ImGuiStateManager* g_imgui_state_manager;

template class MIZU_IMGUI_API BaseStateManager<ImGuiStaticState, ImGuiDynamicState, ImGuiHandle, ImGuiConfig>;

} // namespace Mizu
