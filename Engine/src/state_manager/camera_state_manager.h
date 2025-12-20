#pragma once

#include <glm/glm.hpp>

#include "render/camera.h"

#include "state_manager/base_state_manager.h"

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

extern CameraStateManager* g_camera_state_manager;

void sim_set_camera_state(const Camera& camera);
Camera rend_get_camera_state();

} // namespace Mizu