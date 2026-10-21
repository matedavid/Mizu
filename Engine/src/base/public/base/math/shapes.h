#pragma once

#include <glm/glm.hpp>

#include "mizu_base_module.h"

namespace Mizu
{
namespace math
{

struct AABB
{
    glm::vec3 min{};
    glm::vec3 max{};
};

struct Sphere
{
    glm::vec3 center{};
    float radius = 0.0;
};

MIZU_BASE_API AABB transform_aabb(const AABB& aabb, const glm::mat4& transform);

MIZU_BASE_API Sphere bounding_sphere_from_transformed_aabb(const AABB& local, const glm::mat4& transform);

} // namespace math
} // namespace Mizu