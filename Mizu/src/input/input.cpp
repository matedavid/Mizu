#include "input.h"

#include <GLFW/glfw3.h>

#include "core/application.h"
#include "core/window.h"

#include "utility/logging.h"

namespace Mizu {

inline static bool check_key_pressed(GLFWwindow* window, int32_t key) {
    const auto status = glfwGetKey(window, key);
    return status == GLFW_PRESS || status == GLFW_REPEAT;
}

bool Input::is_key_pressed(Key key) {
    const auto& window = Application::instance()->get_window()->handle();

    const auto glfw_key = static_cast<int32_t>(key);
    return check_key_pressed(window, glfw_key);
}

bool Input::is_modifier_keys_pressed(ModifierKeyBits mods) {
    const auto& window = Application::instance()->get_window()->handle();

    // Convert ModifierKey to Key and check if its pressed
    bool all_pressed = true;

    if (mods & ModifierKeyBits::Shift) {
        all_pressed =
            all_pressed
            && (check_key_pressed(window, GLFW_KEY_LEFT_SHIFT) || check_key_pressed(window, GLFW_KEY_RIGHT_SHIFT));
    }

    if (mods & ModifierKeyBits::Control) {
        all_pressed =
            all_pressed
            && (check_key_pressed(window, GLFW_KEY_LEFT_CONTROL) || check_key_pressed(window, GLFW_KEY_RIGHT_CONTROL));
    }

    if (mods & ModifierKeyBits::Alt) {
        all_pressed =
            all_pressed
            && (check_key_pressed(window, GLFW_KEY_LEFT_ALT) || check_key_pressed(window, GLFW_KEY_RIGHT_ALT));
    }

    if (mods & ModifierKeyBits::Super) {
        all_pressed =
            all_pressed
            && (check_key_pressed(window, GLFW_KEY_LEFT_SUPER) || check_key_pressed(window, GLFW_KEY_RIGHT_SUPER));
    }

    if (mods & ModifierKeyBits::CapsLock) {
        all_pressed = all_pressed && check_key_pressed(window, GLFW_KEY_CAPS_LOCK);
    }

    if (mods & ModifierKeyBits::NumLock) {
        all_pressed = all_pressed && check_key_pressed(window, GLFW_KEY_NUM_LOCK);
    }

    return all_pressed;
}

bool Input::is_mouse_button_pressed(MouseButton button) {
    const auto& window = Application::instance()->get_window()->handle();

    const auto glfw_button = static_cast<int32_t>(button);
    const auto status = glfwGetMouseButton(window, glfw_button);
    return status == GLFW_PRESS;
}

float Input::horizontal_axis_change() {
    const auto& window = Application::instance()->get_window();
    return window->get_mouse_change().x;
}

float Input::vertical_axis_change() {
    const auto& window = Application::instance()->get_window();
    return window->get_mouse_change().y;
}

} // namespace Mizu