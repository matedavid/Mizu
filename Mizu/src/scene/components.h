#pragma once

#include <glm/glm.hpp>
#include <string>

#include "core/uuid.h"
#include "renderer/mesh.h"

namespace Mizu {

struct NameComponent {
    std::string name;
};

struct UUIDComponent {
    UUID id;
};

struct TransformComponent {
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
};

struct MeshRendererComponent {
    std::shared_ptr<Mesh> mesh;
};

} // namespace Mizu
