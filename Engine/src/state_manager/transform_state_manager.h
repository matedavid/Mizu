#pragma once

#include <cstdint>
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

struct TransformConfig : StateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(TransformHandle);

using TransformStateManager =
    BaseStateManager<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig>;

extern TransformStateManager* g_transform_manager;

} // namespace Mizu