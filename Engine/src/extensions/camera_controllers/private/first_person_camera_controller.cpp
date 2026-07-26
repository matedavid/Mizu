#include "camera_controllers/first_person_camera_controller.h"

#include <glm/gtc/matrix_transform.hpp>

#include "core/input.h"

namespace Mizu
{

FirstPersonCameraController::FirstPersonCameraController(float fov, float aspect, float znear, float zfar)
    : m_camera{.fov = fov, .aspect = aspect, .znear = znear, .zfar = zfar}
{
}

void FirstPersonCameraController::set_config(Config config)
{
    m_config = config;
}

void FirstPersonCameraController::update(double ts)
{
    const auto fts = static_cast<float>(ts);

    // Rotation
    if (!m_config.rotate_modifier_key.has_value() || modifier_key_pressed(*m_config.rotate_modifier_key))
    {
        auto change = glm::vec2(Input::horizontal_axis_change(), Input::vertical_axis_change());
        if (glm::length(change) != 0)
        {
            change = glm::normalize(change);
        }

        const float horizontal_change = change.x;
        const float vertical_change = change.y;

        const float pitch = vertical_change * m_config.vertical_rotation_sensitivity * fts;
        const float yaw = horizontal_change * m_config.lateral_rotation_sensitivity * fts;

        set_rotation(m_camera.rotation + glm::vec3(pitch, yaw, 0.0f));
    }

    // The forward vector follows directly from pitch/yaw (roll is unused).
    const float pitch_angle = m_camera.rotation.x;
    const float yaw_angle = m_camera.rotation.y;

    const auto front = glm::vec3(
        glm::cos(pitch_angle) * glm::sin(yaw_angle),
        -glm::sin(pitch_angle),
        -glm::cos(pitch_angle) * glm::cos(yaw_angle));
    const auto right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Position
    if (!m_config.move_modifier_key.has_value() || modifier_key_pressed(*m_config.move_modifier_key))
    {
        glm::vec3 movement(0.0f);

        if (Input::is_key_pressed(Key::W))
        {
            movement += m_config.longitudinal_movement_speed * fts * front;
        }
        else if (Input::is_key_pressed(Key::S))
        {
            movement += m_config.longitudinal_movement_speed * fts * (-front);
        }

        if (Input::is_key_pressed(Key::A))
        {
            movement += m_config.lateral_movement_speed * fts * (-right);
        }
        else if (Input::is_key_pressed(Key::D))
        {
            movement += m_config.lateral_movement_speed * fts * right;
        }

        set_position(m_camera.position + movement);
    }
}

#define CHECK_MODIFIER_VARIANT(type, func)      \
    if (std::holds_alternative<type>(modifier)) \
    {                                           \
        return func(std::get<type>(modifier));  \
    }

bool FirstPersonCameraController::modifier_key_pressed(ModifierKeyT modifier)
{
    CHECK_MODIFIER_VARIANT(MouseButton, Input::is_mouse_button_pressed);
    CHECK_MODIFIER_VARIANT(Key, Input::is_key_pressed);
    CHECK_MODIFIER_VARIANT(ModifierKeyBits, Input::is_modifier_keys_pressed);

    return false;
}

} // namespace Mizu
