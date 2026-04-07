#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <variant>

#include "state_manager/base_state_manager.h"
#include "state_manager/base_state_manager2.h"

#include "mizu_render_module.h"
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

    bool has_changed(const LightDynamicState& other) const
    {
        const bool color_changed = !glm::all(glm::epsilonEqual(color, other.color, glm::epsilon<float>()));
        const bool intensity_changed = intensity != other.intensity;
        const bool cast_shadows_changed = cast_shadows != other.cast_shadows;
        const bool data_changed = variant_changed(other);

        return color_changed || intensity_changed || cast_shadows_changed || data_changed;
    }

    bool variant_changed(const LightDynamicState& other) const
    {
        constexpr float epsilon = glm::epsilon<float>();

        if (data.index() != other.data.index())
            return true;

        if (std::holds_alternative<Point>(data))
        {
            const Point& self_point = std::get<Point>(data);
            const Point& other_point = std::get<Point>(other.data);

            return self_point.radius != other_point.radius;
        }
        else if (std::holds_alternative<Directional>(data))
        {
            const Directional& self_dir = std::get<Directional>(data);
            const Directional& other_dir = std::get<Directional>(other.data);

            return !glm::all(glm::epsilonEqual(self_dir.direction, other_dir.direction, epsilon));
        }

        MIZU_UNREACHABLE("Invalid variant types");
        return false;
    }

    LightDynamicState interpolate(const LightDynamicState& target, double alpha) const
    {
        const float falpha = static_cast<float>(alpha);

        LightDynamicState ds{};
        ds.color = glm::mix(color, target.color, falpha);
        ds.intensity = glm::mix(intensity, target.intensity, falpha);
        ds.cast_shadows = falpha < 0.5f ? cast_shadows : target.cast_shadows;
        ds.data = interpolate_variant(data, target.data, falpha);

        return ds;
    }

    static std::variant<Point, Directional> interpolate_variant(
        const std::variant<Point, Directional>& source,
        const std::variant<Point, Directional>& target,
        float alpha)
    {
        if (source.index() != target.index())
        {
            return alpha > 0.5f ? target : source;
        }
        if (std::holds_alternative<Point>(source))
        {
            return interpolate_variant(std::get<Point>(source), std::get<Point>(target), alpha);
        }
        else if (std::holds_alternative<Directional>(source))
        {
            return interpolate_variant(std::get<Directional>(source), std::get<Directional>(target), alpha);
        }

        MIZU_UNREACHABLE("Invalid variant types");
        return source;
    }

    static Point interpolate_variant(const Point& source, const Point& target, float alpha)
    {
        Point point{};
        point.radius = glm::mix(source.radius, target.radius, alpha);
        return point;
    }

    static Directional interpolate_variant(const Directional& source, const Directional& target, float alpha)
    {
        Directional result{};
        result.direction = glm::normalize(glm::mix(source.direction, target.direction, alpha));
        return result;
    }
};

struct LightConfig : BaseStateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(LightHandle);

struct MIZU_RENDER_API LightHandleFunctions : LightHandle
{
    glm::vec3 get_position() const;

    bool is_point_light() const;
    bool is_directional_light() const;
};

using LightStateManager = BaseStateManager<LightStaticState, LightDynamicState, LightHandle, LightConfig>;

MIZU_RENDER_API extern LightStateManager* g_light_state_manager;

struct LightConfig2 : BaseStateManagerConfig2
{
    static constexpr std::string_view Identifier = "LightStateManager";
};

using LightStateManager2 = BaseStateManager2<LightStaticState, LightDynamicState, LightHandle, LightConfig2>;

MIZU_RENDER_API extern LightStateManager2* g_light_state_manager2;

} // namespace Mizu
