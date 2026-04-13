#include "render/state_manager/light_state_manager.h"

#include "state_manager/base_state_manager2.inl.cpp"

namespace Mizu
{

LightStateManager2* g_light_state_manager2;

template class MIZU_RENDER_API BaseStateManager2<LightStaticState, LightDynamicState, LightHandle, LightConfig2>;

} // namespace Mizu