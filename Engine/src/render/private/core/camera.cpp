#include "render/core/camera.h"

#include <glm/gtc/matrix_access.hpp>

#include "base/math/shapes.h"

namespace Mizu
{

//
// Frustum
//

Frustum Frustum::from_view_projection(const glm::mat4& vp, const glm::vec3& center)
{
    // Frustum creation from: https://gdbooks.gitbooks.io/legacyopengl/content/Chapter8/frustum.html
    const glm::vec4 row1 = glm::row(vp, 0);
    const glm::vec4 row2 = glm::row(vp, 1);
    const glm::vec4 row3 = glm::row(vp, 2);
    const glm::vec4 row4 = glm::row(vp, 3);

    Frustum f;
    f.center = center;

    f.left = {
        .normal = row4 + row1,
        .distance = row4.w + row1.w,
    };

    f.right = {
        .normal = row4 - row1,
        .distance = row4.w - row1.w,
    };

    f.bottom = {
        .normal = row4 + row2,
        .distance = row4.w + row2.w,
    };

    f.top = {
        .normal = row4 - row2,
        .distance = row4.w - row2.w,
    };

    f.near = {
        .normal = row4 + row3,
        .distance = row4.w + row3.w,
    };

    f.far = {
        .normal = row4 - row3,
        .distance = row4.w - row3.w,
    };

    return f;
}

bool Frustum::is_inside_frustum(const math::AABB& aabb, FrustumMask mask) const
{
    const auto test_plane = [&](const Plane& plane, bool enabled) -> bool {
        if (!enabled)
            return true;

        const auto center = (aabb.min + aabb.max) * 0.5f;
        const auto radius = glm::abs(aabb.max - aabb.min) * 0.5f;

        const auto distance = glm::dot(plane.normal, center) + plane.distance;

        return distance >= -glm::dot(glm::abs(plane.normal), radius);
    };

    // clang-format off
    return test_plane(left,   mask.left)
        && test_plane(right,  mask.right)
        && test_plane(bottom, mask.bottom)
        && test_plane(top,    mask.top)
        && test_plane(near,   mask.near)
        && test_plane(far,    mask.far);
    // clang-format on
}

} // namespace Mizu
