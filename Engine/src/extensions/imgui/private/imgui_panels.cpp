#include "imgui/imgui_panels.h"

#include <algorithm>
#include <imgui.h>
#include <vector>

#include "base/debug/assert.h"

namespace Mizu
{

namespace
{

struct ImGuiPanel
{
    std::string_view name;
    std::string_view menu;
    InplaceImGuiPanelJob func;

    ImGuiPanelKind kind = ImGuiPanelKind::Window;
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;
    bool closable = true;

    bool is_open = true;
};

struct ImGuiMenuItems
{
    std::string_view name;
    std::string_view menu;
    InplaceImGuiPanelJob func;
};

std::vector<ImGuiPanel> s_panels{};
std::vector<ImGuiMenuItems> s_menu_items{};

std::vector<std::string_view> s_menu_order{};

bool s_executing = false;
ImGuiPanel* s_current_panel = nullptr;

template <typename T>
typename std::vector<T>::iterator find_entry(std::vector<T>& entries, std::string_view name)
{
    return std::find_if(entries.begin(), entries.end(), [name](const T& entry) { return entry.name == name; });
}

template <typename T>
bool entry_exists(std::vector<T>& entries, std::string_view name)
{
    return find_entry(entries, name) != entries.end();
}

template <typename T>
void insert_sorted(std::vector<T>& entries, T&& entry)
{
    const auto it = std::lower_bound(
        entries.begin(), entries.end(), entry.name, [](const T& e, std::string_view name) { return e.name < name; });

    entries.insert(it, std::move(entry));
}

template <typename T>
void unregister_entry(std::vector<T>& entries, std::string_view name)
{
    const auto it = find_entry(entries, name);
    if (it == entries.end())
        return;

    entries.erase(it);
}

void add_menu(std::string_view menu)
{
    if (menu.empty())
        return;

    if (std::find(s_menu_order.begin(), s_menu_order.end(), menu) != s_menu_order.end())
        return;

    s_menu_order.push_back(menu);
}

void draw_menu(std::string_view menu)
{
    if (!ImGui::BeginMenu(menu.data()))
        return;

    bool has_panel_items = false;
    for (ImGuiPanel& panel : s_panels)
    {
        if (panel.menu != menu)
            continue;

        ImGui::MenuItem(panel.name.data(), nullptr, &panel.is_open);
        has_panel_items = true;
    }

    bool separator_drawn = false;
    for (ImGuiMenuItems& items : s_menu_items)
    {
        if (items.menu != menu)
            continue;

        if (has_panel_items && !separator_drawn)
        {
            ImGui::Separator();
            separator_drawn = true;
        }

        items.func();
    }

    ImGui::EndMenu();
}

void draw_main_menu_bar()
{
    s_menu_order.clear();

    for (const ImGuiPanel& panel : s_panels)
    {
        add_menu(panel.menu);
    }

    for (const ImGuiMenuItems& items : s_menu_items)
    {
        add_menu(items.menu);
    }

    if (s_menu_order.empty())
        return;

    std::sort(s_menu_order.begin(), s_menu_order.end());

    if (!ImGui::BeginMainMenuBar())
        return;

    for (const std::string_view menu : s_menu_order)
    {
        draw_menu(menu);
    }

    ImGui::EndMainMenuBar();
}

void execute_panel(ImGuiPanel& panel)
{
    switch (panel.kind)
    {
    case ImGuiPanelKind::Window: {
        bool* is_open = panel.closable ? &panel.is_open : nullptr;

        if (ImGui::Begin(panel.name.data(), is_open, panel.window_flags))
            panel.func();

        ImGui::End();

        break;
    }
    case ImGuiPanelKind::Custom: {
        panel.func();

        break;
    }
    }
}

} // namespace

void ImGuiPanels::register_panel(const ImGuiPanelBuilder& builder, InplaceImGuiPanelJob&& job)
{
    MIZU_ASSERT(!s_executing, "Can't register ImGui panel {} while panels are executing", builder.m_name);
    MIZU_ASSERT(
        !entry_exists(s_panels, builder.m_name),
        "ImGui panel with name {} has already been registered",
        builder.m_name);
    MIZU_ASSERT(
        builder.m_kind == ImGuiPanelKind::Window || builder.m_window_flags == ImGuiWindowFlags_None,
        "Window flags are only used by Window panels, panel {} is Custom",
        builder.m_name);

    insert_sorted(
        s_panels,
        ImGuiPanel{
            .name = builder.m_name,
            .menu = builder.m_menu,
            .func = std::move(job),
            .kind = builder.m_kind,
            .window_flags = builder.m_window_flags,
            .closable = builder.m_closable,
            .is_open = builder.m_open_by_default,
        });
}

void ImGuiPanels::unregister_panel(std::string_view name)
{
    MIZU_ASSERT(!s_executing, "Can't unregister ImGui panel {} while panels are executing", name);
    unregister_entry(s_panels, name);
}

void ImGuiPanels::set_panel_open(std::string_view name, bool open)
{
    const auto it = find_entry(s_panels, name);
    MIZU_ASSERT(it != s_panels.end(), "ImGui panel with name {} does not exist", name);

    if (it == s_panels.end())
        return;

    it->is_open = open;
}

void ImGuiPanels::register_menu_items(std::string_view name, std::string_view menu, InplaceImGuiPanelJob&& job)
{
    MIZU_ASSERT(!s_executing, "Can't register ImGui menu items {} while panels are executing", name);
    MIZU_ASSERT(!entry_exists(s_menu_items, name), "ImGui menu items with name {} have already been registered", name);
    MIZU_ASSERT(!menu.empty(), "ImGui menu items with name {} must have a menu", name);

    insert_sorted(
        s_menu_items,
        ImGuiMenuItems{
            .name = name,
            .menu = menu,
            .func = std::move(job),
        });
}

void ImGuiPanels::unregister_menu_items(std::string_view name)
{
    MIZU_ASSERT(!s_executing, "Can't unregister ImGui menu items {} while panels are executing", name);
    unregister_entry(s_menu_items, name);
}

bool* ImGuiPanels::current_open()
{
    MIZU_ASSERT(s_current_panel != nullptr, "current_open can only be called from inside a panel callback");
    return &s_current_panel->is_open;
}

void ImGuiPanels::execute()
{
    s_executing = true;

    draw_main_menu_bar();

    for (ImGuiPanel& panel : s_panels)
    {
        if (!panel.is_open)
            continue;

        s_current_panel = &panel;
        execute_panel(panel);
        s_current_panel = nullptr;
    }

    s_executing = false;
}

} // namespace Mizu
