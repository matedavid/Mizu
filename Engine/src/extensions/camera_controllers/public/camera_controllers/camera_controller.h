#pragma once

#include "render/core/camera.h"

namespace Mizu
{

class CameraController
{
  public:
    virtual ~CameraController() = default;

    virtual void set_position(glm::vec3 position) = 0;
    virtual void set_rotation(glm::vec3 rotation) = 0;
    virtual void set_aspect_ratio(float aspect) = 0;
    virtual void set_fov(float fov) = 0;

    virtual Camera get_camera() const = 0;
};

} // namespace Mizu