#pragma once

#include <imgui.h>

#include "mizu_imgui_module.h"

namespace Mizu
{

class Window;

class MIZU_IMGUI_API ImGuiContext
{
  public:
    static void init(const Window& window);
    static void shutdown();

    static void init_simulation_state();

    static void new_frame();
    static void render();
};

} // namespace Mizu