#pragma once

#include <glm/glm.hpp>

#include "base/math/geometric.h"
#include "base/math/shapes.h"

#include "registries/render_view_registry.h"

namespace Mizu
{

glm::vec2 viewport_pixel_size(const ViewportRect& viewport, glm::uvec2 output_size)
{
    return {
        std::round(viewport.extent.x * static_cast<float>(output_size.x)),
        std::round(viewport.extent.y * static_cast<float>(output_size.y)),
    };
}

float pixels_per_world_unit(const ViewportRect& viewport, float fov, float depth, glm::uvec2 viewport_size)
{
    const float height = viewport_pixel_size(viewport, viewport_size).y;
    const float tan_half_fov_y = glm::tan(fov / 2.0f);

    return height / (2.0f * depth * tan_half_fov_y);
}

float projected_area_fraction(const math::Sphere& sphere, const RenderViewRegistryEntry& view, glm::uvec2 viewport_size)
{
    const glm::vec2 viewport_px = viewport_pixel_size(view.viewport, viewport_size);
    const float screen_px = viewport_px.x * viewport_px.y;
    if (screen_px <= 0.0f)
        return 0.0f;

    if (glm::distance(sphere.center, view.camera.position) <= sphere.radius)
        return 1.0f;

    const float center_depth = -(view.view_matrix * glm::vec4(sphere.center, 1.0f)).z;

    if (center_depth + sphere.radius <= view.camera.znear)
        return 0.0f;

    const float depth = std::max(center_depth, view.camera.znear);

    const float radius_px = sphere.radius * pixels_per_world_unit(view.viewport, view.camera.fov, depth, viewport_size);
    const float area_px = glm::pi<float>() * radius_px * radius_px;

    return glm::clamp(area_px / screen_px, 0.0f, 1.0f);
}

} // namespace Mizu