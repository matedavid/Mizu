#pragma once

#include <glm/glm.hpp>
#include <string>

#include "core/uuid.h"
#include "render_core/resources/material.h"
#include "render_core/resources/mesh.h"

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
    glm::vec3 scale;
};

struct MeshRendererComponent
{
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
};

} // namespace Mizu
