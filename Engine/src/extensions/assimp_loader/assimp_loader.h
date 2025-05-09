#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace Mizu
{

// Forward declarations
class Mesh;
class Material;

class AssimpLoader
{
  public:
    static std::optional<AssimpLoader> load(std::filesystem::path path);

    [[nodiscard]] const std::vector<std::shared_ptr<Mesh>>& get_meshes() const { return m_meshes; }
    [[nodiscard]] const std::vector<std::shared_ptr<Material>>& get_materials() const { return m_materials; }

  private:
    std::filesystem::path m_path;
    std::filesystem::path m_container_folder;

    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;

    AssimpLoader() = default;
    [[nodiscard]] bool load_internal(std::filesystem::path path);
};

} // namespace Mizu