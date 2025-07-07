#pragma once

#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"

namespace Mizu
{

struct CameraStaticState
{
};

struct CameraDynamicState
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 pos;
};

struct CameraConfig : StateManagerConfig
{
    static constexpr uint32_t MaxNumHandles = 2;
};

MIZU_STATE_MANAGER_CREATE_HANDLE(CameraHandle);

using CameraStateManager = BaseStateManager<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig>;

extern CameraStateManager* g_camera_state_manager;

void sim_set_camera_state(CameraDynamicState dynamic_state);
CameraDynamicState rend_get_camera_state();

} // namespace Mizu