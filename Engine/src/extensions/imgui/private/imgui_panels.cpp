#include "imgui/imgui_panels.h"

#include <imgui.h>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

namespace
{

struct ImGuiPanel
{
    ImGuiPanelHandle handle;
    std::string_view name;
    InplaceImGuiPanelJob func;

    bool is_open;
};

std::vector<ImGuiPanel> s_imgui_panels{};

bool imgui_panel_exists(std::string_view name)
{
    for (const ImGuiPanel& panel : s_imgui_panels)
    {
        if (panel.name == name)
            return true;
    }

    return false;
}

} // namespace

ImGuiPanelHandle ImGuiPanels::add(std::string_view name, InplaceImGuiPanelJob&& func)
{
    MIZU_ASSERT(!imgui_panel_exists(name), "ImGui panel with name {} has already been registered", name);

    ImGuiPanel& panel = s_imgui_panels.emplace_back();
    panel.handle = ImGuiPanelHandle{}; // TODO:
    panel.name = name;
    panel.func = std::move(func);
    panel.is_open = true;

    return panel.handle;
}

void ImGuiPanels::remove(ImGuiPanelHandle handle)
{
    (void)handle;
}

void ImGuiPanels::execute()
{
    for (ImGuiPanel& panel : s_imgui_panels)
    {
        ImGui::Begin(panel.name.data(), &panel.is_open);

        panel.func();

        ImGui::End();
    }
}

} // namespace Mizu
