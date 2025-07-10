#pragma once

#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"

namespace Mizu
{

struct TransformStaticState
{
};

struct TransformDynamicState
{
    glm::vec3 translation{0.0};
    glm::vec3 rotation{0.0};
    glm::vec3 scale{1.0f};
};

struct TransformConfig : BaseStateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(TransformHandle);

struct TransformHandleFunctions : public TransformHandle
{
    glm::vec3 get_translation() const;
    void set_translation(const glm::vec3& translation);

    glm::vec3 get_rotation() const;
    void set_rotation(const glm::vec3& rotation);

    glm::vec3 get_scale() const;
    void set_scale(const glm::vec3& scale);
};

using TransformStateManager =
    BaseStateManager<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig>;

extern TransformStateManager* g_transform_state_manager;

} // namespace Mizu