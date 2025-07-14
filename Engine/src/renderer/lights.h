#pragma once

#include <glm/glm.hpp>

namespace Mizu
{

struct PointLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    bool cast_shadows;
};

struct DirectionalLight
{
    glm::vec3 position;
    float intensity;
    glm::vec3 color;
    bool cast_shadows;
    glm::vec3 direction;

    float _padding;
};

using GPUPointLight = PointLight;
using GPUDirectionalLight = DirectionalLight;

} // namespace Mizu
