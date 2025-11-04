#include "camera.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "base/math/bounding_box.h"
#include "render_core/rhi/renderer.h"

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

bool Frustum::is_inside_frustum(const BBox& aabb, FrustumMask mask) const
{
    const auto test_plane = [](const BBox& _aabb, const Plane& plane, bool enabled) -> bool {
        if (!enabled)
            return true;

        const auto center = (_aabb.min() + _aabb.max()) * 0.5f;
        const auto radius = glm::abs(_aabb.max() - _aabb.min()) * 0.5f;

        const auto distance = glm::dot(plane.normal, center) + plane.distance;

        return distance >= -glm::dot(glm::abs(plane.normal), radius);
    };

    // clang-format off
    return test_plane(aabb, left,   mask.left)
        && test_plane(aabb, right,  mask.right)
        && test_plane(aabb, bottom, mask.bottom)
        && test_plane(aabb, top,    mask.top)
        && test_plane(aabb, near,   mask.near)
        && test_plane(aabb, far,    mask.far);
    // clang-format on
}

//
// Camera
//

void Camera::set_position(glm::vec3 position)
{
    m_position = position;
    recalculate_view_matrix();
}

void Camera::set_rotation(glm::vec3 rotation)
{
    m_rotation = rotation;
    recalculate_view_matrix();
}

void Camera::recalculate_view_matrix()
{
    m_view = glm::mat4(1.0f);
    m_view = glm::translate(m_view, -m_position);
    m_view = glm::rotate(m_view, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    m_view = glm::rotate(m_view, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    m_view = glm::rotate(m_view, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));

    recalculate_frustum();
}

bool Camera::is_inside_frustum(const BBox& aabb, FrustumMask mask) const
{
    return m_frustum.is_inside_frustum(aabb, mask);
}

//
// PerspectiveCamera
//

PerspectiveCamera::PerspectiveCamera() : PerspectiveCamera(glm::radians(90.0f), 1.0f, 0.001f, 100.0f) {}

PerspectiveCamera::PerspectiveCamera(float fov, float aspect, float znear, float zfar) : m_fov(fov), m_aspect(aspect)
{
    // TODO: Why can't I just initialize it from the constructor initializers...
    m_znear = znear;
    m_zfar = zfar;

    recalculate_view_matrix();
    set_aspect_ratio(aspect);
}

void PerspectiveCamera::set_aspect_ratio(float aspect)
{
    m_aspect = aspect;

    const float tmp_fov = m_fov;
    if (m_aspect < 1.0f)
    {
        m_fov = 2.0f * glm::atan(glm::tan(m_fov * 0.5f) / m_aspect);
    }

    recalculate_projection_matrix();
    m_fov = tmp_fov;
}

void PerspectiveCamera::recalculate_projection_matrix()
{
    m_projection = glm::perspectiveRH_ZO(m_fov, m_aspect, m_znear, m_zfar);

    if (Renderer::get_config().graphics_api == GraphicsAPI::Vulkan)
    {
        m_projection[1][1] *= -1.0f;
    }

    recalculate_frustum();
}

void PerspectiveCamera::recalculate_frustum()
{
    const glm::mat4 VP = m_projection * m_view;
    m_frustum = Frustum::from_view_projection(VP, m_position);
}

} // namespace Mizu
