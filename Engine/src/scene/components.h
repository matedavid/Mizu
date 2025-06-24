#pragma once

#include <glm/glm.hpp>
#include <string>

#include "base/types/uuid.h"

#include "renderer/material/material.h"
#include "renderer/model/mesh.h"

namespace Mizu
{

struct NameComponent
{
    std::string name;
};

struct UUIDComponent
{
    UUID id;
};

struct TransformComponent
{
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale = glm::vec3(1.0f);
};

struct MeshRendererComponent
{
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
};

struct PointLightComponent
{
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
};

struct DirectionalLightComponent
{
    glm::vec3 color = glm::vec3(1.0f);
    float intensity = 1.0f;
    bool cast_shadows = false;
};

} // namespace Mizu
