#include "render/state_manager/transform_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

TransformStateManager* g_transform_state_manager;

template class MIZU_RENDER_API
    BaseStateManager<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig>;

} // namespace Mizu
