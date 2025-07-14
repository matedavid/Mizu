#pragma once

#include <glm/glm.hpp>
#include <variant>

#include "state_manager/base_state_manager.h"
#include "state_manager/transform_state_manager.h"

namespace Mizu
{

struct LightStaticState
{
    TransformHandle transform_handle;
};

struct LightDynamicState
{
    glm::vec3 color;
    float intensity;
    bool cast_shadows;

    struct Point
    {
        float radius;
    };

    struct Directional
    {
        glm::vec3 direction;
    };

    std::variant<Point, Directional> data;
};

struct LightConfig : BaseStateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(LightHandle);

struct LightHandleFunctions : LightHandle
{
    glm::vec3 get_position() const;

    bool is_point_light() const;
    bool is_directional_light() const;
};

using LightStateManager = BaseStateManager<LightStaticState, LightDynamicState, LightHandle, LightConfig>;

extern LightStateManager* g_light_state_manager;

} // namespace Mizu