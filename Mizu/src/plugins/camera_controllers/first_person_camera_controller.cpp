#include "first_person_camera_controller.h"

#include <glm/gtc/matrix_transform.hpp>

#include "input/input.h"

namespace Mizu {

FirstPersonCameraController::FirstPersonCameraController() : PerspectiveCamera() {}

FirstPersonCameraController::FirstPersonCameraController(float fov, float aspect, float znear, float zfar)
      : PerspectiveCamera(fov, aspect, znear, zfar) {}

void FirstPersonCameraController::set_config(Config config) {
    m_config = config;
}

void FirstPersonCameraController::update(double ts) {
    const auto fts = static_cast<float>(ts);

    // Position
    if (m_config.move_modifier_key == ModifierKeyBits::None
        || Input::is_modifier_keys_pressed(m_config.move_modifier_key)) {
        glm::vec3 movement(0.0f);

        if (Input::is_key_pressed(Key::W)) {
            movement += m_config.longitudinal_movement_speed * fts * glm::vec3(0.0f, 0.0f, -1.0f);
        } else if (Input::is_key_pressed(Key::S)) {
            movement += m_config.longitudinal_movement_speed * fts * glm::vec3(0.0f, 0.0f, 1.0f);
        }

        if (Input::is_key_pressed(Key::A)) {
            movement += m_config.lateral_movement_speed * fts * glm::vec3(-1.0f, 0.0f, 0.0f);
        } else if (Input::is_key_pressed(Key::D)) {
            movement += m_config.lateral_movement_speed * fts * glm::vec3(1.0f, 0.0f, 0.0f);
        }

        set_position(m_position + movement);
    }

    // Rotation
    if (m_config.rotate_modifier_key == ModifierKeyBits::None
        || Input::is_modifier_keys_pressed(m_config.rotate_modifier_key)) {
        const float horizontal_change = Input::horizontal_axis_change();
        const float vertical_change = Input::vertical_axis_change();

        const float pitch = horizontal_change * m_config.lateral_rotation_sensitivity * fts;
        const float yaw = vertical_change * m_config.vertical_rotation_sensitivity * fts;

        const glm::vec3 new_rotation = m_rotation + glm::vec3(-yaw, pitch, 0.0f);
        set_rotation(new_rotation);
    }
}

void FirstPersonCameraController::recalculate_view_matrix() {
    m_view = glm::mat4(1.0f);
    m_view = glm::rotate(m_view, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    m_view = glm::rotate(m_view, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    m_view = glm::rotate(m_view, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
    m_view = glm::translate(m_view, -m_position);
}

} // namespace Mizu