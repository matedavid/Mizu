#include "render/state_manager/camera_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

CameraStateManager* g_camera_state_manager;

template class BaseStateManager<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig>;

static CameraHandle s_camera_handle;

void sim_set_camera_state(const Camera& camera)
{
    if (!s_camera_handle.is_valid())
    {
        s_camera_handle = g_camera_state_manager->sim_create_handle(CameraStaticState{}, CameraDynamicState{});
    }

    g_camera_state_manager->sim_edit_dynamic_state(s_camera_handle).camera = camera;
}

Camera rend_get_camera_state()
{
    MIZU_ASSERT(s_camera_handle.is_valid(), "Camera handle is not valid");

    return g_camera_state_manager->rend_get_dynamic_state(s_camera_handle).camera;
}

} // namespace Mizu