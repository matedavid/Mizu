#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

struct GpuPointLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float cast_shadows;
    float radius;

    float _padding[3];
};

static_assert(sizeof(GpuPointLight) == 48, "PointLight size must be 48 bytes to match Gpu layout");

struct GpuDirectionalLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float cast_shadows;
    glm::vec3 direction;

    float _padding;
};

static_assert(sizeof(GpuDirectionalLight) == 48, "DirectionalLight size must be 48 bytes to match Gpu layout");

} // namespace Mizu
