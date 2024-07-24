#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Mizu {

class BBox {
  public:
    BBox();
    BBox(glm::vec3 min, glm::vec3 max);
    explicit BBox(const std::vector<glm::vec3>& values);

    [[nodiscard]] glm::vec3 min() const { return m_min; }
    [[nodiscard]] glm::vec3 max() const { return m_max; }

  private:
    glm::vec3 m_min;
    glm::vec3 m_max;
};

} // namespace Mizu
