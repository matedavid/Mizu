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

    glm::vec4 to_vec4() const { return glm::vec4(normal, distance); }
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

    uint8_t to_uint8() const
    {
        // clang-format off
        return static_cast<uint8_t>(
            (top    << 0u)
          | (bottom << 1u)
          | (left   << 2u)
          | (right  << 3u)
          | (near   << 4u)
          | (far    << 5u)
        );
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

    glm::vec3 center{0.0f};

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

struct Camera
{
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);

    float fov = glm::radians(60.0f);
    float aspect = 1.0f;

    float znear = 0.001f;
    float zfar = 100.0f;
};

} // namespace Mizu
