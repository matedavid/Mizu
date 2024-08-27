#pragma once

#include <glm/glm.hpp>

namespace Mizu {

class Camera {
  public:
    virtual ~Camera() = default;

    void set_position(glm::vec3 position);
    void set_rotation(glm::vec3 rotation);

    [[nodiscard]] const glm::mat4& view_matrix() const { return m_view; }
    [[nodiscard]] virtual const glm::mat4& projection_matrix() const = 0;

    [[nodiscard]] const glm::vec3& get_position() const { return m_position; }
    [[nodiscard]] const glm::vec3& get_rotation() const { return m_rotation; }

  protected:
    glm::mat4 m_view{};
    glm::mat4 m_projection{};

    glm::vec3 m_position{};
    glm::vec3 m_rotation{};

    virtual void recalculate_view_matrix();
};

class PerspectiveCamera : public Camera {
  public:
    PerspectiveCamera();
    PerspectiveCamera(float fov, float aspect, float znear, float zfar);
    virtual ~PerspectiveCamera() override = default;

    void set_aspect_ratio(float aspect);

    [[nodiscard]] const glm::mat4& projection_matrix() const override { return m_projection; }

  protected:
    float m_fov, m_aspect, m_znear, m_zfar;

    virtual void recalculate_projection_matrix();
};

} // namespace Mizu
