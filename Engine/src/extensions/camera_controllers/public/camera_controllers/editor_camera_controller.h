#pragma once

#include "render/camera.h"

#include "mizu_camera_controllers_module.h"

namespace Mizu
{

class MIZU_CAMERA_CONTROLLERS_API EditorCameraController : public PerspectiveCamera
{
  public:
    EditorCameraController();
    EditorCameraController(float fov, float aspect, float znear, float zfar);
    ~EditorCameraController() override = default;

    void update(double ts);

    float get_speed() const { return m_speed; }

  private:
    float m_speed = 5.0f;
    float m_sensitivity = 5.0f;

    void recalculate_view_matrix() override;
};

} // namespace Mizu