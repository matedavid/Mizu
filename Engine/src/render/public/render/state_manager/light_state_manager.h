#pragma once

#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

#include "state_manager/base_state_manager2.h"
#include "state_manager/state_manager_consumer.h"

#include "mizu_render_module.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

enum class LightType
{
    Point,
    Directional,
};

struct LightStaticState
{
    TransformHandle transform_handle;
    LightType type = LightType::Point;
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

    union Data {
        Point point;
        Directional directional;

        constexpr Data() : point{} {}

        ~Data() = default;
    } data{};

    bool has_changed(const LightDynamicState& other) const
    {
        const bool color_changed = !glm::all(glm::epsilonEqual(color, other.color, glm::epsilon<float>()));
        const bool intensity_changed = intensity != other.intensity;
        const bool cast_shadows_changed = cast_shadows != other.cast_shadows;
        const bool data_changed = std::memcmp(&data, &other.data, sizeof(Data)) != 0;

        return color_changed || intensity_changed || cast_shadows_changed || data_changed;
    }
};

MIZU_STATE_MANAGER_CREATE_HANDLE(LightHandle);

struct LightConfig2 : BaseStateManagerConfig2
{
    static constexpr std::string_view Identifier = "LightStateManager";
};

using LightStateManager2 = BaseStateManager2<LightStaticState, LightDynamicState, LightHandle, LightConfig2>;
using LightStateManagerConsumer = IStateManagerConsumer<LightStateManager2>;

MIZU_RENDER_API extern LightStateManager2* g_light_state_manager2;

} // namespace Mizu
