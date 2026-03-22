#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

struct PointLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float cast_shadows;
    float radius;

    float _padding[3];
};

static_assert(sizeof(PointLight) == 48, "PointLight size must be 48 bytes to match GPU layout");

struct DirectionalLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float cast_shadows;
    glm::vec3 direction;

    float _padding;
};

static_assert(sizeof(DirectionalLight) == 48, "DirectionalLight size must be 48 bytes to match GPU layout");

using GpuPointLight = PointLight;
using GpuDirectionalLight = DirectionalLight;

} // namespace Mizu
