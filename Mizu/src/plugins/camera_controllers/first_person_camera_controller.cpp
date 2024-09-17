#include "first_person_camera_controller.h"

#include <glm/gtc/matrix_transform.hpp>

#include "input/input.h"
#include "renderer/abstraction/renderer.h"
#include "utility/logging.h"

namespace Mizu {

FirstPersonCameraController::FirstPersonCameraController() : PerspectiveCamera() {}

FirstPersonCameraController::FirstPersonCameraController(float fov, float aspect, float znear, float zfar)
      : PerspectiveCamera(fov, aspect, znear, zfar) {}

void FirstPersonCameraController::set_config(Config config) {
    m_config = config;
}

void FirstPersonCameraController::update(double ts) {
    const auto fts = static_cast<float>(ts);

    // Rotation
    if (!m_config.rotate_modifier_key.has_value() || modifier_key_pressed(*m_config.rotate_modifier_key)) {
        const float horizontal_change = Input::horizontal_axis_change();
        const float vertical_change = Input::vertical_axis_change();

        float pitch = vertical_change * m_config.vertical_rotation_sensitivity * fts;
        float yaw = horizontal_change * m_config.lateral_rotation_sensitivity * fts;

        set_rotation(m_rotation + glm::vec3(pitch, yaw, 0.0f));
    }

    const auto front = glm::normalize(glm::vec3(-m_view[0][2], -m_view[1][2], -m_view[2][2]));
    const auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Position
    if (!m_config.move_modifier_key.has_value() || modifier_key_pressed(*m_config.move_modifier_key)) {
        glm::vec3 movement(0.0f);

        if (Input::is_key_pressed(Key::W)) {
            movement += m_config.longitudinal_movement_speed * fts * front;
        } else if (Input::is_key_pressed(Key::S)) {
            movement += m_config.longitudinal_movement_speed * fts * (-front);
        }

        if (Input::is_key_pressed(Key::A)) {
            movement += m_config.lateral_movement_speed * fts * (-right);
        } else if (Input::is_key_pressed(Key::D)) {
            movement += m_config.lateral_movement_speed * fts * right;
        }

        set_position(m_position + movement);
    }
}

void FirstPersonCameraController::recalculate_view_matrix() {
    m_view = glm::mat4(1.0f);

    m_view = glm::rotate(m_view, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f)); // Pitch
    m_view = glm::rotate(m_view, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f)); // Yaw
    m_view = glm::rotate(m_view, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f)); // Roll
    m_view = glm::translate(m_view, -m_position);
}

#define CHECK_MODIFIER_VARIANT(type, func)        \
    if (std::holds_alternative<type>(modifier)) { \
        return func(std::get<type>(modifier));    \
    }

bool FirstPersonCameraController::modifier_key_pressed(ModifierKeyT modifier) {
    CHECK_MODIFIER_VARIANT(MouseButton, Input::is_mouse_button_pressed);
    CHECK_MODIFIER_VARIANT(Key, Input::is_key_pressed);
    CHECK_MODIFIER_VARIANT(ModifierKeyBits, Input::is_modifier_keys_pressed);

    return false;
}

} // namespace Mizu