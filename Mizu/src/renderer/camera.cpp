#include "camera.h"

#include "renderer/abstraction/renderer.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Mizu {

//
// Camera
//

void Camera::set_position(glm::vec3 position) {
    m_position = position;
    recalculate_view_matrix();
}

void Camera::set_rotation(glm::vec3 rotation) {
    m_rotation = rotation;
    recalculate_view_matrix();
}

void Camera::recalculate_view_matrix() {
    m_view = glm::mat4(1.0f);
    m_view = glm::translate(m_view, -m_position);
    m_view = glm::rotate(m_view, m_rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
    m_view = glm::rotate(m_view, m_rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
    m_view = glm::rotate(m_view, m_rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
}

//
// PerspectiveCamera
//

PerspectiveCamera::PerspectiveCamera() : PerspectiveCamera(glm::radians(90.0f), 1.0f, 0.001f, 100.0f) {}

PerspectiveCamera::PerspectiveCamera(float fov, float aspect, float znear, float zfar)
      : m_fov(fov), m_aspect(aspect), m_znear(znear), m_zfar(zfar) {
    recalculate_view_matrix();
    set_aspect_ratio(aspect);
}

void PerspectiveCamera::set_aspect_ratio(float aspect) {
    m_aspect = aspect;

    const float tmp_fov = m_fov;
    if (aspect < 1.0f) {
        m_fov = 2.0f * glm::atan(glm::tan(m_fov * 0.5f) / aspect);
    }
    recalculate_projection_matrix();
    m_fov = tmp_fov;
}

void PerspectiveCamera::recalculate_projection_matrix() {
    const GraphicsAPI graphics_api = Renderer::get_config().graphics_api;

    if (graphics_api == GraphicsAPI::OpenGL) {
        m_projection = glm::perspective(m_fov, m_aspect, m_znear, m_zfar);
    } else if (graphics_api == GraphicsAPI::Vulkan) {
        m_projection = glm::perspectiveRH_ZO(m_fov, m_aspect, m_znear, m_zfar);
    }

    if (graphics_api == GraphicsAPI::Vulkan) {
        m_projection[1][1] *= -1;
    }
}

} // namespace Mizu
