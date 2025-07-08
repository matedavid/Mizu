#include "camera.h"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "base/math/bounding_box.h"
#include "render_core/rhi/renderer.h"

namespace Mizu
{

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

bool Camera::is_inside_frustum(const BBox& aabb) const
{
    const auto test_plane = [](const BBox& _aabb, const Plane& plane) -> bool {
        const auto center = (_aabb.min() + _aabb.max()) * 0.5f;
        const auto radius = glm::abs(_aabb.max() - _aabb.min()) * 0.5f;

        const auto distance = glm::dot(plane.normal, center) + plane.distance;

        return distance >= -glm::dot(glm::abs(plane.normal), radius);
    };

    // clang-format off
    return test_plane(aabb, m_frustum.left)
        && test_plane(aabb, m_frustum.right)
        && test_plane(aabb, m_frustum.bottom)
        && test_plane(aabb, m_frustum.top)
        && test_plane(aabb, m_frustum.near)
        && test_plane(aabb, m_frustum.far);
    // clang-format on
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
    if (aspect < 1.0f)
    {
        m_fov = 2.0f * glm::atan(glm::tan(m_fov * 0.5f) / aspect);
    }
    recalculate_projection_matrix();
    m_fov = tmp_fov;
}

void PerspectiveCamera::recalculate_projection_matrix()
{
    const GraphicsAPI graphics_api = Renderer::get_config().graphics_api;

    if (graphics_api == GraphicsAPI::OpenGL)
    {
        m_projection = glm::perspectiveRH_NO(m_fov, m_aspect, m_znear, m_zfar);
    }
    else if (graphics_api == GraphicsAPI::Vulkan)
    {
        m_projection = glm::perspectiveRH_ZO(m_fov, m_aspect, m_znear, m_zfar);
        m_projection[1][1] *= -1.0f;
    }

    recalculate_frustum();
}

void PerspectiveCamera::recalculate_frustum()
{
    const glm::mat4 VP = m_projection * m_view;

    // Frustum creation from: https://gdbooks.gitbooks.io/legacyopengl/content/Chapter8/frustum.html
    const glm::vec4 row1 = glm::row(VP, 0);
    const glm::vec4 row2 = glm::row(VP, 1);
    const glm::vec4 row3 = glm::row(VP, 2);
    const glm::vec4 row4 = glm::row(VP, 3);

    m_frustum.left = {
        .normal = row4 + row1,
        .distance = row4.w + row1.w,
    };

    m_frustum.right = {
        .normal = row4 - row1,
        .distance = row4.w - row1.w,
    };

    m_frustum.bottom = {
        .normal = row4 + row2,
        .distance = row4.w + row2.w,
    };

    m_frustum.top = {
        .normal = row4 - row2,
        .distance = row4.w - row2.w,
    };

    m_frustum.near = {
        .normal = row4 + row3,
        .distance = row4.w + row3.w,
    };

    m_frustum.far = {
        .normal = row4 - row3,
        .distance = row4.w - row3.w,
    };
}

} // namespace Mizu
