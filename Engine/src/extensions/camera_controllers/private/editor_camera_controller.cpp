#include "camera_controllers/editor_camera_controller.h"

#include "core/input.h"

namespace Mizu
{

EditorCameraController::EditorCameraController(float fov, float aspect, float znear, float zfar)
    : m_camera{.fov = fov, .aspect = aspect, .znear = znear, .zfar = zfar}
{
}

void EditorCameraController::update(double ts)
{
    const float fts = static_cast<float>(ts);

    // Rotation
    if (Input::is_mouse_button_pressed(MouseButton::Right))
    {
        auto change = glm::vec2(Input::horizontal_axis_change(), Input::vertical_axis_change());
        if (glm::length(change) != 0)
        {
            change = glm::normalize(change);
        }

        const float horizontal_change = change.x;
        const float vertical_change = change.y;

        const float pitch = vertical_change * m_sensitivity * fts;
        const float yaw = horizontal_change * m_sensitivity * fts;

        set_rotation(m_camera.rotation + glm::vec3(pitch, yaw, 0.0f));
    }

    // The forward vector follows directly from pitch/yaw (roll is unused).
    const float pitch_angle = m_camera.rotation.x;
    const float yaw_angle = m_camera.rotation.y;

    const glm::vec3 front = glm::vec3(
        glm::cos(pitch_angle) * glm::sin(yaw_angle),
        -glm::sin(pitch_angle),
        -glm::cos(pitch_angle) * glm::cos(yaw_angle));
    const glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Position
    {
        glm::vec3 movement(0.0f);

        if (Input::is_key_pressed(Key::W))
        {
            movement += m_speed * fts * front;
        }
        else if (Input::is_key_pressed(Key::S))
        {
            movement += m_speed * fts * (-front);
        }

        if (Input::is_key_pressed(Key::A))
        {
            movement += m_speed * fts * (-right);
        }
        else if (Input::is_key_pressed(Key::D))
        {
            movement += m_speed * fts * right;
        }

        set_position(m_camera.position + movement);
    }

    // Mouse button scroll
    if (Input::is_mouse_button_pressed(MouseButton::Right))
    {
        const glm::vec2 scroll_change = Input::mouse_scroll_change();

        constexpr float CHANGE_FACTOR = 1.0f;
        constexpr float MIN_SPEED = 0.1f;
        constexpr float MAX_SPEED = 100.0f;

        m_speed = glm::clamp(m_speed + scroll_change.y * CHANGE_FACTOR, MIN_SPEED, MAX_SPEED);
    }
}

} // namespace Mizu