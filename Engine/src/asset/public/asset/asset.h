#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace Mizu
{

constexpr size_t MaxAssetMounts = 5;

struct AssetMount
{
    std::filesystem::path path;
    std::string name;
};

struct MeshAssetVertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
};

} // namespace Mizu