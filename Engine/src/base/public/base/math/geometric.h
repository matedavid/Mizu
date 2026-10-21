#pragma once

#include <glm/glm.hpp>

namespace Mizu
{
namespace math
{

glm::vec3 axis_scales(const glm::mat4& mat)
{
    return glm::vec3{
        glm::length(glm::vec3(mat[0])),
        glm::length(glm::vec3(mat[1])),
        glm::length(glm::vec3(mat[2])),
    };
}

float max_axis_scale(const glm::mat4& mat)
{
    const glm::vec3 s = axis_scales(mat);
    return glm::max(s.x, glm::max(s.y, s.z));
}

} // namespace math
} // namespace Mizu