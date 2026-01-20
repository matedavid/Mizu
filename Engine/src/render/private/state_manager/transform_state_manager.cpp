#include "render/state_manager/transform_state_manager.h"

#include "state_manager/base_state_manager.inl.cpp"

namespace Mizu
{

glm::vec3 TransformHandleFunctions::get_translation() const
{
    return g_transform_state_manager->get_dynamic_state(*this).translation;
}

void TransformHandleFunctions::set_translation(const glm::vec3& translation)
{
    g_transform_state_manager->edit_dynamic_state(*this).translation = translation;
}

glm::vec3 TransformHandleFunctions::get_rotation() const
{
    return g_transform_state_manager->get_dynamic_state(*this).rotation;
}

void TransformHandleFunctions::set_rotation(const glm::vec3& rotation)
{
    g_transform_state_manager->edit_dynamic_state(*this).rotation = rotation;
}

glm::vec3 TransformHandleFunctions::get_scale() const
{
    return g_transform_state_manager->get_dynamic_state(*this).scale;
}

void TransformHandleFunctions::set_scale(const glm::vec3& scale)
{
    g_transform_state_manager->edit_dynamic_state(*this).scale = scale;
}

TransformStateManager* g_transform_state_manager;

template class MIZU_RENDER_API
    BaseStateManager<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig>;

} // namespace Mizu