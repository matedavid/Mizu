#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

struct PointLight
{
    glm::vec4 position; // vec4 for padding
    glm::vec4 color;    // vec4 for padding
    float intensity;

    float _padding[3];
};

} // namespace Mizu