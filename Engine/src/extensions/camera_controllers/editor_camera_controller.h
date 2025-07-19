#pragma once

#include "renderer/camera.h"

namespace Mizu
{

class EditorCameraController : public PerspectiveCamera
{
  public:
    EditorCameraController();
    EditorCameraController(float fov, float aspect, float znear, float zfar);
    ~EditorCameraController() override = default;

    void update(double ts);

  private:
    float m_speed = 5.0f;
    float m_sensitivity = 5.0f;

    void recalculate_view_matrix() override;
};

} // namespace Mizu