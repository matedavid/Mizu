#pragma once

#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"

#include "render/camera.h"

namespace Mizu
{

struct CameraStaticState
{
};

struct CameraDynamicState
{
    Camera camera;
};

struct CameraConfig : BaseStateManagerConfig
{
    static constexpr uint32_t MaxNumHandles = 2;
};

MIZU_STATE_MANAGER_CREATE_HANDLE(CameraHandle);

using CameraStateManager = BaseStateManager<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig>;

MIZU_RENDER_API extern CameraStateManager* g_camera_state_manager;

MIZU_RENDER_API void sim_set_camera_state(const Camera& camera);
MIZU_RENDER_API Camera rend_get_camera_state();

} // namespace Mizu