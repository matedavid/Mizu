#pragma once

#include "input/keycodes.h"
#include "input/events.h"
#include "renderer/camera.h"

namespace Mizu {

class FirstPersonCameraController : public PerspectiveCamera {
  public:
    struct Config {
        float lateral_movement_speed = 1.0f;
        float longitudinal_movement_speed = 1.0f;

        ModifierKeyBits move_modifier_key = ModifierKeyBits::None;

        float lateral_rotation_sensitivity = 1.0f;
        float vertical_rotation_sensitivity = 1.0f;

        ModifierKeyBits rotate_modifier_key = ModifierKeyBits::None;
    };

    FirstPersonCameraController();
    FirstPersonCameraController(float fov, float aspect, float znear, float zfar);
    ~FirstPersonCameraController() override = default;

    void set_config(Config config);
    void update(double ts);

  private:
    Config m_config;

    void recalculate_view_matrix() override;
};

} // namespace Mizu
