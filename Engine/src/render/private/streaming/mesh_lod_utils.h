#pragma once

#include <glm/glm.hpp>

#include "base/math/shapes.h"

namespace Mizu
{

struct MeshLodView
{
    // TODO: Only works with perspective projections, orthographic views need a screen size without the depth division

    glm::vec3 position{0.0f};
    float projection_scale = 1.0f;
    glm::vec3 forward{0.0f, 0.0f, -1.0f};
    float znear = 0.001f;
};

inline MeshLodView create_mesh_lod_view(const glm::mat4& view_matrix, const glm::mat4& proj_matrix, float znear)
{
    return MeshLodView{
        .position = glm::vec3(glm::inverse(view_matrix)[3]),
        .projection_scale = glm::abs(proj_matrix[1][1]),
        .forward = -glm::vec3(view_matrix[0][2], view_matrix[1][2], view_matrix[2][2]),
        .znear = znear,
    };
}

inline float compute_mesh_screen_size(const math::Sphere& sphere, const MeshLodView& view)
{
    const float depth = glm::max(glm::dot(sphere.center - view.position, view.forward), view.znear);
    return sphere.radius * view.projection_scale / depth;
}

} // namespace Mizu
