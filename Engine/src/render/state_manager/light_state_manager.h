#pragma once

#include <glm/glm.hpp>
#include <variant>

#include "state_manager/base_state_manager.h"

#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

struct LightStaticState
{
    TransformHandle transform_handle;
};

struct LightDynamicState
{
    glm::vec3 color = glm::vec3(0.0f);
    float intensity = 1.0f;
    bool cast_shadows = true;

    struct Point
    {
        float radius = 10.0f;
    };

    struct Directional
    {
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, 1.0f);
    };

    std::variant<Point, Directional> data = Point{};
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