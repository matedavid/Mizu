#include "base/math/shapes.h"

#include "base/math/geometric.h"

namespace Mizu
{
namespace math
{

AABB transform_aabb(const AABB& aabb, const glm::mat4& transform)
{
    const glm::vec3 center = (aabb.min + aabb.max) * 0.5f;
    const glm::vec3 extent = (aabb.max - aabb.min) * 0.5f;

    glm::vec3 new_center = glm::vec3(transform[3]);
    glm::vec3 new_extent = glm::vec3(0.0f);

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            new_center[i] += transform[j][i] * center[j];
            new_extent[i] += glm::abs(transform[j][i]) * extent[j];
        }
    }

    return AABB(new_center - new_extent, new_center + new_extent);
}

Sphere bounding_sphere_from_transformed_aabb(const math::AABB& local, const glm::mat4& transform)
{
    const glm::vec3 local_center = 0.5f * (local.min + local.max);
    const glm::vec3 local_half = 0.5f * (local.max - local.min);

    const float radius = glm::length(local_half * math::axis_scales(transform));

    return Sphere{
        .center = glm::vec3(transform * glm::vec4(local_center, 1.0f)),
        .radius = radius,
    };
}

} // namespace math
} // namespace Mizu