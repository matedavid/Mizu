#pragma once

#include <optional>
#include <variant>

#include "core/keycodes.h"

#include "camera_controllers/camera_controller.h"
#include "mizu_camera_controllers_module.h"

namespace Mizu
{

class MIZU_CAMERA_CONTROLLERS_API FirstPersonCameraController : public CameraController
{
  public:
    using ModifierKeyT = std::variant<ModifierKeyBits, MouseButton, Key>;

    struct Config
    {
        float lateral_movement_speed = 1.0f;
        float longitudinal_movement_speed = 1.0f;

        std::optional<ModifierKeyT> move_modifier_key = std::nullopt;

        float lateral_rotation_sensitivity = 1.0f;
        float vertical_rotation_sensitivity = 1.0f;

        std::optional<ModifierKeyT> rotate_modifier_key = std::nullopt;
    };

    FirstPersonCameraController() = default;
    FirstPersonCameraController(float fov, float aspect, float znear, float zfar);
    ~FirstPersonCameraController() override = default;

    void set_position(glm::vec3 position) override { m_camera.position = position; }
    void set_rotation(glm::vec3 rotation) override { m_camera.rotation = rotation; }
    void set_aspect_ratio(float aspect) override { m_camera.aspect = aspect; }
    void set_fov(float fov) override { m_camera.fov = fov; }

    Camera get_camera() const override { return m_camera; }

    void set_config(Config config);
    void update(double ts);

  private:
    Camera m_camera{};
    Config m_config{};

    static bool modifier_key_pressed(ModifierKeyT modifier);
};

} // namespace Mizu
