#include "static_mesh_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

StaticMeshStateManager* g_static_mesh_state_manager;

template class BaseStateManager<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig>;

} // namespace Mizu