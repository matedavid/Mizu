#include "bounding_box.h"

namespace Mizu {

BBox::BBox() {
    m_min = glm::vec3(std::numeric_limits<float>::infinity());
    m_max = glm::vec3(-std::numeric_limits<float>::infinity());
}

BBox::BBox(glm::vec3 min, glm::vec3 max) : m_min(min), m_max(max) {}

BBox::BBox(const std::vector<glm::vec3>& values) : BBox() {
    for (const auto& value : values) {
        m_max = glm::max(value, m_max);
        m_min = glm::min(value, m_min);
    }
}

} // namespace Mizu
