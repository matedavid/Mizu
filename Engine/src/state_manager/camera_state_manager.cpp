#include "camera_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

CameraStateManager* g_camera_state_manager;

template class BaseStateManager<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig>;

static CameraHandle s_camera_handle;

void sim_set_camera_state(CameraDynamicState dynamic_state)
{
    if (!s_camera_handle.is_valid())
    {
        s_camera_handle = g_camera_state_manager->sim_create_handle(CameraStaticState{}, dynamic_state);
    }

    g_camera_state_manager->sim_update(s_camera_handle, dynamic_state);
}

CameraDynamicState rend_get_camera_state()
{
    MIZU_ASSERT(s_camera_handle.is_valid(), "Camera handle is not valid");

    return g_camera_state_manager->rend_get_dynamic_state(s_camera_handle);
}

} // namespace Mizu