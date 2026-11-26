#pragma once

#include <string_view>

#include "base/containers/inplace_function.h"

#include "mizu_imgui_module.h"

namespace Mizu
{

constexpr size_t InplaceImGuiPanelMemoryBytes = 48;
using InplaceImGuiPanelJob = inplace_function<InplaceImGuiPanelMemoryBytes>;

struct ImGuiPanelHandle
{
};

class MIZU_IMGUI_API ImGuiPanels
{
  public:
    static ImGuiPanelHandle add(std::string_view name, InplaceImGuiPanelJob&& func);
    static void remove(ImGuiPanelHandle handle);

    static void execute();
};

} // namespace Mizu