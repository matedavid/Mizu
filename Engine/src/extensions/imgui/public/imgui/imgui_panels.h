#pragma once

#include <imgui.h>
#include <string_view>

#include "base/containers/inplace_function.h"

#include "mizu_imgui_module.h"

namespace Mizu
{

constexpr size_t InplaceImGuiPanelMemoryBytes = 48;
using InplaceImGuiPanelJob = inplace_function<InplaceImGuiPanelMemoryBytes>;

enum class ImGuiPanelKind
{
    Window,
    Custom,
};

class ImGuiPanelBuilder
{
  public:
    static ImGuiPanelBuilder window(std::string_view name) { return ImGuiPanelBuilder(name, ImGuiPanelKind::Window); }
    static ImGuiPanelBuilder custom(std::string_view name) { return ImGuiPanelBuilder(name, ImGuiPanelKind::Custom); }

    ImGuiPanelBuilder& menu(std::string_view menu)
    {
        m_menu = menu;
        return *this;
    }

    ImGuiPanelBuilder& no_menu()
    {
        m_menu = {};
        return *this;
    }

    ImGuiPanelBuilder& no_close()
    {
        m_closable = false;
        return *this;
    }

    ImGuiPanelBuilder& closed()
    {
        m_open_by_default = false;
        return *this;
    }

    ImGuiPanelBuilder& flags(ImGuiWindowFlags flags)
    {
        m_window_flags = flags;
        return *this;
    }

  private:
    ImGuiPanelBuilder(std::string_view name, ImGuiPanelKind kind) : m_name(name), m_kind(kind) {}

    std::string_view m_name{};
    std::string_view m_menu = "Panels";
    ImGuiPanelKind m_kind = ImGuiPanelKind::Window;
    ImGuiWindowFlags m_window_flags = ImGuiWindowFlags_None;

    bool m_closable = true;
    bool m_open_by_default = true;

    friend class ImGuiPanels;
};

class MIZU_IMGUI_API ImGuiPanels
{
  public:
    template <typename Func>
    static void register_panel(std::string_view name, Func&& func)
    {
        register_panel(ImGuiPanelBuilder::window(name), InplaceImGuiPanelJob::create(std::forward<Func>(func)));
    }

    template <typename Func>
    static void register_panel(const ImGuiPanelBuilder& builder, Func&& func)
    {
        register_panel(builder, InplaceImGuiPanelJob::create(std::forward<Func>(func)));
    }

    static void register_panel(const ImGuiPanelBuilder& builder, InplaceImGuiPanelJob&& job);
    static void unregister_panel(std::string_view name);

    static void set_panel_open(std::string_view name, bool open);

    template <typename Func>
    static void register_menu_items(std::string_view name, std::string_view menu, Func&& func)
    {
        register_menu_items(name, menu, InplaceImGuiPanelJob::create(std::forward<Func>(func)));
    }

    static void register_menu_items(std::string_view name, std::string_view menu, InplaceImGuiPanelJob&& job);
    static void unregister_menu_items(std::string_view name);

    static bool* current_open();

    static void execute();
};

} // namespace Mizu
