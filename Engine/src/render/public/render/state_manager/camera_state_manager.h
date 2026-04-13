#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>

#include "state_manager/base_state_manager.h"

#include "render/core/camera.h"

namespace Mizu
{

struct CameraStaticState
{
};

struct CameraDynamicState
{
    Camera camera;

    bool has_changed(const CameraDynamicState& other) const
    {
        constexpr float epsilon = glm::epsilon<float>();

        const auto vec3_equal = [&](const glm::vec3& a, const glm::vec3& b) {
            return glm::all(glm::epsilonEqual(a, b, epsilon));
        };

        const auto mat4_equal = [&](const glm::mat4& a, const glm::mat4& b) {
            for (int column = 0; column < 4; ++column)
            {
                if (!glm::all(glm::epsilonEqual(a[column], b[column], epsilon)))
                    return false;
            }

            return true;
        };

        const bool position_has_changed = !vec3_equal(camera.get_position(), other.camera.get_position());
        const bool rotation_has_changed = !vec3_equal(camera.get_rotation(), other.camera.get_rotation());
        const bool znear_has_changed = camera.get_znear() != other.camera.get_znear();
        const bool zfar_has_changed = camera.get_zfar() != other.camera.get_zfar();
        const bool view_matrix_has_changed = !mat4_equal(camera.get_view_matrix(), other.camera.get_view_matrix());
        const bool projection_matrix_has_changed =
            !mat4_equal(camera.get_projection_matrix(), other.camera.get_projection_matrix());

        return position_has_changed || rotation_has_changed || znear_has_changed || zfar_has_changed
               || view_matrix_has_changed || projection_matrix_has_changed;
    }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(CameraHandle);

struct CameraConfig : BaseStateManagerConfig
{
    static constexpr uint64_t MaxNumHandles = 2;
    static constexpr bool Interpolate = false;

    static constexpr std::string_view Identifier = "CameraStateManager";
};

using CameraStateManager = BaseStateManager<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig>;

MIZU_RENDER_API extern CameraStateManager* g_camera_state_manager;

MIZU_RENDER_API void sim_set_camera_state(const Camera& camera);
MIZU_RENDER_API const Camera& rend_get_camera_state();

} // namespace Mizu