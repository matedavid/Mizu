#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>

#include "mizu_render_module.h"

namespace Mizu
{

class AABB;

struct Plane
{
    glm::vec3 normal{};
    float distance = 0.0f;

    bool operator==(const Plane& other) const
    {
        constexpr float EPSILON = glm::epsilon<float>();

        return glm::all(glm::epsilonEqual(normal, other.normal, EPSILON))
               && glm::epsilonEqual(distance, other.distance, EPSILON);
    }
};

struct FrustumMask
{
    bool top : 1 = true;
    bool bottom : 1 = true;
    bool left : 1 = true;
    bool right : 1 = true;
    bool near : 1 = true;
    bool far : 1 = true;

    bool operator==(const FrustumMask& other) const
    {
        // clang-format off
        return top    == other.top 
            && bottom == other.bottom 
            && left   == other.left 
            && right  == other.right
            && near   == other.near 
            && far    == other.far;
        // clang-format on
    }
};

static_assert(sizeof(FrustumMask) == 1, "Size of FrustumMask must be 1 byte");

struct Frustum
{
    Plane top{};
    Plane bottom{};
    Plane left{};
    Plane right{};
    Plane near{};
    Plane far{};

    glm::vec3 center;

    static Frustum from_view_projection(const glm::mat4& vp, const glm::vec3& center);

    bool is_inside_frustum(const AABB& aabb, FrustumMask mask = {}) const;

    bool operator==(const Frustum& other) const
    {
        // clang-format off
        return top    == other.top
            && bottom == other.bottom
            && left   == other.left
            && right  == other.right
            && near   == other.near
            && far    == other.far
            && glm::all(glm::epsilonEqual(center, other.center, glm::epsilon<float>()));
        // clang-format on
    }
};

class MIZU_RENDER_API Camera
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

    bool is_inside_frustum(const AABB& aabb, FrustumMask mask = {}) const;

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

class MIZU_RENDER_API PerspectiveCamera : public Camera
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

struct Camera2
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);

    float fov = glm::radians(60.0f);
    float aspect = 1.0f;

    float znear = 0.001f;
    float zfar = 100.0f;
};

} // namespace Mizu
