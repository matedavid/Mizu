#include "render/state_manager/camera_state_manager.h"

#include "state_manager/base_state_manager2.inl.cpp"

namespace Mizu
{

CameraStateManager2* g_camera_state_manager2;

template class MIZU_RENDER_API BaseStateManager2<CameraStaticState, CameraDynamicState, CameraHandle, CameraConfig2>;

static CameraHandle s_camera_handle;

void sim_set_camera_state(const Camera& camera)
{
    if (!s_camera_handle.is_valid())
    {
        s_camera_handle = g_camera_state_manager2->sim_create(CameraStaticState{}, CameraDynamicState{});
    }

    g_camera_state_manager2->sim_edit(s_camera_handle).camera = camera;
}

const Camera& rend_get_camera_state()
{
    if (!s_camera_handle.is_valid())
    {
        static Camera s_default_camera;
        return s_default_camera;
    }

    return g_camera_state_manager2->rend_get_dynamic_state(s_camera_handle).camera;
}

} // namespace Mizu
