#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/epsilon.hpp>

#include "state_manager/base_state_manager.h"
#include "state_manager/base_state_manager2.h"

#include "mizu_render_module.h"

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

    bool has_changed(const TransformDynamicState& previous) const
    {
        constexpr float epsilon = glm::epsilon<float>();
        constexpr float pi = glm::pi<float>();

        const auto vec3_equal = [&](const glm::vec3& a, const glm::vec3& b, float eps) {
            return glm::all(glm::epsilonEqual(a, b, eps));
        };

        const auto wrap_angle = [&](float angle) -> float {
            angle = std::fmod(angle, 2.0f * pi);
            if (angle < -pi)
                angle += 2.0f * pi;
            if (angle >= pi)
                angle -= 2.0f * pi;

            return angle;
        };

        const auto wrap_euler = [&](const glm::vec3& euler) -> glm::vec3 {
            return {wrap_angle(euler.x), wrap_angle(euler.y), wrap_angle(euler.z)};
        };

        const bool translation_changed = !vec3_equal(translation, previous.translation, epsilon);
        const bool rotation_changed = !vec3_equal(wrap_euler(rotation), wrap_euler(previous.rotation), epsilon);
        const bool scale_changed = !vec3_equal(scale, previous.scale, epsilon);

        return translation_changed || rotation_changed || scale_changed;
    }

    TransformDynamicState interpolate(const TransformDynamicState& target, double alpha) const
    {
        const float falpha = static_cast<float>(alpha);

        TransformDynamicState ds{};
        ds.translation = glm::mix(translation, target.translation, falpha);
        ds.rotation = glm::mix(rotation, target.rotation, falpha);
        ds.scale = glm::mix(scale, target.scale, falpha);

        return ds;
    }
};

struct TransformConfig : BaseStateManagerConfig
{
};

MIZU_STATE_MANAGER_CREATE_HANDLE(TransformHandle);

struct MIZU_RENDER_API TransformHandleFunctions : public TransformHandle
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

MIZU_RENDER_API extern TransformStateManager* g_transform_state_manager;

struct TransformConfig2 : BaseStateManagerConfig2
{
    static constexpr uint64_t MaxNumHandles = 1000;
    static constexpr bool Interpolate = true;

    static constexpr std::string_view Identifier = "TransformStateManager";
};

using TransformStateManager2 =
    BaseStateManager2<TransformStaticState, TransformDynamicState, TransformHandle, TransformConfig2>;

MIZU_RENDER_API extern TransformStateManager2* g_transform_state_manager2;

} // namespace Mizu
