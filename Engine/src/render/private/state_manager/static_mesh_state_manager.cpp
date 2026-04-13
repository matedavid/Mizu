#include "render/state_manager/static_mesh_state_manager.h"

#include "state_manager/base_state_manager2.inl.cpp"

namespace Mizu
{

StaticMeshStateManager2* g_static_mesh_state_manager2;

template class MIZU_RENDER_API
    BaseStateManager2<StaticMeshStaticState, StaticMeshDynamicState, StaticMeshHandle, StaticMeshConfig2>;

} // namespace Mizu
