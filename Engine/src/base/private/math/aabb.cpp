#include "base/math/aabb.h"

namespace Mizu
{

AABB::AABB()
{
    m_min = glm::vec3(std::numeric_limits<float>::infinity());
    m_max = glm::vec3(-std::numeric_limits<float>::infinity());
}

AABB::AABB(glm::vec3 min, glm::vec3 max) : m_min(min), m_max(max) {}

AABB::AABB(const std::vector<glm::vec3>& values) : AABB()
{
    for (const auto& value : values)
    {
        m_max = glm::max(value, m_max);
        m_min = glm::min(value, m_min);
    }
}

AABB transform_aabb(const AABB& aabb, const glm::mat4& m)
{
    const glm::vec3 center = (aabb.min() + aabb.max()) * 0.5f;
    const glm::vec3 extent = (aabb.max() - aabb.min()) * 0.5f;

    glm::vec3 new_center = glm::vec3(m[3]);
    glm::vec3 new_extent = glm::vec3(0.0f);

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            new_center[i] += m[j][i] * center[j];
            new_extent[i] += glm::abs(m[j][i]) * extent[j];
        }
    }

    return AABB(new_center - new_extent, new_center + new_extent);
}

} // namespace Mizu
