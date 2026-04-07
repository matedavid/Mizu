#include "render/state_manager/light_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"
#include "state_manager/base_state_manager2.inl.cpp"

namespace Mizu
{

glm::vec3 LightHandleFunctions::get_position() const
{
    const LightStaticState& static_state = g_light_state_manager->get_static_state(*this);
    return static_state.transform_handle->get_translation();
}

bool LightHandleFunctions::is_point_light() const
{
    return std::holds_alternative<LightDynamicState::Point>(g_light_state_manager->get_dynamic_state(*this).data);
}

bool LightHandleFunctions::is_directional_light() const
{
    return std::holds_alternative<LightDynamicState::Directional>(g_light_state_manager->get_dynamic_state(*this).data);
}

LightStateManager* g_light_state_manager;

template class MIZU_RENDER_API BaseStateManager<LightStaticState, LightDynamicState, LightHandle, LightConfig>;

LightStateManager2* g_light_state_manager2;

template class MIZU_RENDER_API BaseStateManager2<LightStaticState, LightDynamicState, LightHandle, LightConfig2>;

} // namespace Mizu