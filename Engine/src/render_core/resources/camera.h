#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

class Camera
{
  public:
    virtual ~Camera() = default;

    virtual void set_position(glm::vec3 position);
    virtual void set_rotation(glm::vec3 rotation);

    [[nodiscard]] glm::mat4 view_matrix() const { return m_view; }
    [[nodiscard]] virtual glm::mat4 projection_matrix() const = 0;

    [[nodiscard]] glm::vec3 get_position() const { return m_position; }
    [[nodiscard]] glm::vec3 get_rotation() const { return m_rotation; }
    [[nodiscard]] float get_znear() const { return m_znear; }
    [[nodiscard]] float get_zfar() const { return m_zfar; }

  protected:
    glm::mat4 m_view{};
    glm::mat4 m_projection{};

    glm::vec3 m_position{};
    glm::vec3 m_rotation{};

    float m_znear, m_zfar;

    virtual void recalculate_view_matrix();
};

class PerspectiveCamera : public Camera
{
  public:
    PerspectiveCamera();
    PerspectiveCamera(float fov, float aspect, float znear, float zfar);
    virtual ~PerspectiveCamera() override = default;

    void set_aspect_ratio(float aspect);

    [[nodiscard]] glm::mat4 projection_matrix() const override { return m_projection; }
    [[nodiscard]] float get_fov() const { return m_fov; }

  protected:
    float m_fov, m_aspect;

    virtual void recalculate_projection_matrix();
};

} // namespace Mizu
