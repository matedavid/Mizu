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

struct DirectionalLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    float cast_shadows;
    glm::vec3 direction;

    float _padding;
};

using GpuPointLight = PointLight;
using GpuDirectionalLight = DirectionalLight;

} // namespace Mizu
