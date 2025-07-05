#include "transform_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

TransformStateManager* g_transform_state_manager = new TransformStateManager;

template class BaseStateManager<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig>;

} // namespace Mizu