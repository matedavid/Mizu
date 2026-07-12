#include "render/state_manager/render_view_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

RenderViewStateManager* g_render_view_state_manager;

template class MIZU_RENDER_API
    BaseStateManager<RenderViewStaticState, RenderViewDynamicState, RenderViewHandle, RenderViewConfig>;

} // namespace Mizu