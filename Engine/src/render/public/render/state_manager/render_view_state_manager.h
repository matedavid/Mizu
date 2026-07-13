#pragma once

#include <cstdint>
#include <glm/glm.hpp>

#include "state_manager/base_state_manager.h"
#include "state_manager/state_manager.h"

#include "mizu_render_module.h"
#include "render/core/camera.h"

namespace Mizu
{

struct RenderViewStaticState
{
};

struct ViewportRect
{
    glm::vec2 offset{0.0f, 0.0f};
    glm::vec2 extent{1.0f, 1.0f};
};

struct RenderViewDynamicState
{
    ViewportRect viewport{};
    Camera2 camera{};
    uint32_t layer = 0;

    bool has_changed(const RenderViewDynamicState& other) const
    {
        constexpr float epsilon = glm::epsilon<float>();

        const auto vec2_equal = [&](const glm::vec2& a, const glm::vec2& b) {
            return glm::all(glm::epsilonEqual(a, b, epsilon));
        };

        const auto vec3_equal = [&](const glm::vec3& a, const glm::vec3& b) {
            return glm::all(glm::epsilonEqual(a, b, epsilon));
        };

        const bool viewport_changed = [&]() {
            return !vec2_equal(viewport.offset, other.viewport.offset)
                   || !vec2_equal(viewport.extent, other.viewport.extent);
        }();

        const bool camera_changed = [&]() {
            const bool camera_position_changed = !vec3_equal(camera.position, other.camera.position);
            const bool camera_rotation_changed = !vec3_equal(camera.rotation, other.camera.rotation);
            const bool camera_fov_changed = camera.fov != other.camera.fov;
            const bool camera_aspect_changed = camera.aspect != other.camera.aspect;
            const bool camera_znear_changed = camera.znear != other.camera.znear;
            const bool camera_zfar_changed = camera.zfar != other.camera.zfar;

            return camera_position_changed || camera_rotation_changed || camera_fov_changed || camera_aspect_changed
                   || camera_znear_changed || camera_zfar_changed;
        }();

        const bool layer_changed = layer != other.layer;

        return viewport_changed || camera_changed || layer_changed;
    }

    RenderViewDynamicState interpolate(const RenderViewDynamicState& target, double alpha) const
    {
        return RenderViewDynamicState{
            .viewport = interpolate_viewport(viewport, target.viewport, alpha),
            .camera = interpolate_camera(camera, target.camera, alpha),
            .layer = alpha >= 0.5 ? target.layer : layer,
        };
    };

  private:
    static ViewportRect interpolate_viewport(const ViewportRect& source, const ViewportRect& target, double alpha)
    {
        return {
            .offset = glm::mix(source.offset, target.offset, alpha),
            .extent = glm::mix(source.extent, target.extent, alpha),
        };
    }

    static Camera2 interpolate_camera(const Camera2& source, const Camera2& target, double alpha)
    {
        return {
            .position = glm::mix(source.position, target.position, alpha),
            .rotation = glm::mix(source.rotation, target.rotation, alpha),
            .fov = glm::mix(source.fov, target.fov, alpha),
            .aspect = glm::mix(source.aspect, target.fov, alpha),
            .znear = glm::mix(source.znear, target.znear, alpha),
            .zfar = glm::mix(source.zfar, target.zfar, alpha),
        };
    }
};

struct RenderViewConfig : BaseStateManagerConfig
{
    static constexpr uint64_t MaxNumHandles = 10;
    static constexpr bool Interpolate = true;

    static constexpr std::string_view Identifier = "RenderViewStateManager";
};

MIZU_STATE_MANAGER_CREATE_HANDLE(RenderViewHandle);

using RenderViewStateManager =
    BaseStateManager<RenderViewStaticState, RenderViewDynamicState, RenderViewHandle, RenderViewConfig>;
using RenderViewStateManagerConsumer = IStateManagerConsumer<RenderViewStateManager>;

MIZU_RENDER_API extern RenderViewStateManager* g_render_view_state_manager;

} // namespace Mizu