#pragma once

#include <optional>
#include <variant>

#include "input/events.h"
#include "input/keycodes.h"
#include "render_core/resources/camera.h"

namespace Mizu
{

class FirstPersonCameraController : public PerspectiveCamera
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

    [[nodiscard]] static bool modifier_key_pressed(ModifierKeyT modifier);
};

} // namespace Mizu
