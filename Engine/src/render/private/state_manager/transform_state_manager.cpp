#include "render/state_manager/transform_state_manager.h"

#include "state_manager/base_state_manager2.inl.cpp"

namespace Mizu
{

TransformStateManager2* g_transform_state_manager2;

template class MIZU_RENDER_API
    BaseStateManager2<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig2>;

} // namespace Mizu
