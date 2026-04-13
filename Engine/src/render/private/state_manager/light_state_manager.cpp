#include "render/state_manager/light_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

LightStateManager* g_light_state_manager;

template class MIZU_RENDER_API BaseStateManager<LightStaticState, LightDynamicState, LightHandle, LightConfig>;

} // namespace Mizu