#include "render/state_manager/static_mesh_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

StaticMeshStateManager* g_static_mesh_state_manager;

template class MIZU_RENDER_API
    BaseStateManager<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig>;

} // namespace Mizu