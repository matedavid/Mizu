#pragma once

#include <glm/glm.hpp>
#include <string>

#include "core/uuid.h"

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

} // namespace Mizu