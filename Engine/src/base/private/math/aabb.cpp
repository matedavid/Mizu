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

} // namespace Mizu
