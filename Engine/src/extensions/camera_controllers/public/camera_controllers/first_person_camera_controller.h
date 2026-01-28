#pragma once

#include <optional>
#include <variant>

#include "core/keycodes.h"
#include "render/camera.h"

#include "mizu_camera_controllers_module.h"

namespace Mizu
{

class MIZU_CAMERA_CONTROLLERS_API FirstPersonCameraController : public PerspectiveCamera
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

    FirstPersonCameraController();
    FirstPersonCameraController(float fov, float aspect, float znear, float zfar);
    ~FirstPersonCameraController() override = default;

    void set_config(Config config);
    void update(double ts);

  private:
    Config m_config;

    void recalculate_view_matrix() override;

    static bool modifier_key_pressed(ModifierKeyT modifier);
};

} // namespace Mizu
