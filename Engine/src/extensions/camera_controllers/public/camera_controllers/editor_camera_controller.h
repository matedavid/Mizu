#pragma once

#include "camera_controllers/camera_controller.h"
#include "mizu_camera_controllers_module.h"

namespace Mizu
{

class MIZU_CAMERA_CONTROLLERS_API EditorCameraController : public CameraController
{
  public:
    EditorCameraController() = default;
    EditorCameraController(float fov, float aspect, float znear, float zfar);
    ~EditorCameraController() override = default;

    void set_position(glm::vec3 position) override { m_camera.position = position; }
    void set_rotation(glm::vec3 rotation) override { m_camera.rotation = rotation; }
    void set_aspect_ratio(float aspect) override { m_camera.aspect = aspect; }
    void set_fov(float fov) override { m_camera.fov = fov; }

    Camera get_camera() const override { return m_camera; }

    void update(double ts);

    float get_speed() const { return m_speed; }

  private:
    Camera m_camera{};

    float m_speed = 5.0f;
    float m_sensitivity = 5.0f;
};

} // namespace Mizu