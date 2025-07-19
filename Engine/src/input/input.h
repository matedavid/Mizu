#pragma once

#include <glm/glm.hpp>

#include "input/keycodes.h"

namespace Mizu
{

class Input
{
  public:
    static bool is_key_pressed(Key key);
    static bool is_modifier_keys_pressed(ModifierKeyBits mods);
    static bool is_mouse_button_pressed(MouseButton button);

    static float horizontal_axis_change();
    static float vertical_axis_change();

    static glm::vec2 mouse_scroll_change();
};

}; // namespace Mizu
