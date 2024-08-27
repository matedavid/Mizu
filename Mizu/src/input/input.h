#pragma once

#include "input/keycodes.h"

namespace Mizu {

class Input {
  public:
    [[nodiscard]] static bool is_key_pressed(Key key);
    [[nodiscard]] static bool is_modifier_keys_pressed(ModifierKeyBits mods);
    [[nodiscard]] static bool is_mouse_button_pressed(MouseButton button);

    [[nodiscard]] static float horizontal_axis_change();
    [[nodiscard]] static float vertical_axis_change();
};

}; // namespace Mizu
