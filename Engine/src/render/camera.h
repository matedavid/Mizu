#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

// Forward declarations
class BBox;

struct Plane
{
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

struct FrustumMask
{
    bool top : 1 = true;
    bool bottom : 1 = true;
    bool left : 1 = true;
    bool right : 1 = true;
    bool near : 1 = true;
    bool far : 1 = true;
};

struct Frustum
{
    Plane top;
    Plane bottom;
    Plane left;
    Plane right;
    Plane near;
    Plane far;

    glm::vec3 center;

    static Frustum from_view_projection(const glm::mat4& vp, const glm::vec3& center);

    bool is_inside_frustum(const BBox& aabb, FrustumMask mask = {}) const;
};

class Camera
{
  public:
    virtual ~Camera() = default;

    virtual void set_position(glm::vec3 position);
    virtual void set_rotation(glm::vec3 rotation);

    glm::mat4 get_view_matrix() const { return m_view; }
    virtual glm::mat4 get_projection_matrix() const { return m_projection; }

    glm::vec3 get_position() const { return m_position; }
    glm::vec3 get_rotation() const { return m_rotation; }
    float get_znear() const { return m_znear; }
    float get_zfar() const { return m_zfar; }

    bool is_inside_frustum(const BBox& aabb, FrustumMask mask = {}) const;

  protected:
    glm::mat4 m_view{};
    glm::mat4 m_projection{};

    Frustum m_frustum;

    glm::vec3 m_position{};
    glm::vec3 m_rotation{};

    float m_znear, m_zfar;

    virtual void recalculate_view_matrix();
    virtual void recalculate_frustum() {}
};

class PerspectiveCamera : public Camera
{
  public:
    PerspectiveCamera();
    PerspectiveCamera(float fov, float aspect, float znear, float zfar);

    void set_aspect_ratio(float aspect);

    float get_fov() const { return m_fov; }

  protected:
    float m_fov, m_aspect;

    virtual void recalculate_projection_matrix();
    virtual void recalculate_frustum() override;
};

} // namespace Mizu
