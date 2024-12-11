#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace Mizu
{

// Forward declarations
class Mesh;

class AssimpLoader
{
  public:
    static std::optional<AssimpLoader> load(std::filesystem::path path);

    [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& get_meshes() const { return m_meshes; }

  private:
    std::filesystem::path m_path;

    std::vector<std::shared_ptr<Mesh>> m_meshes;

    AssimpLoader() = default;
    [[nodiscard]] bool load_internal(std::filesystem::path path);
};

} // namespace Mizu