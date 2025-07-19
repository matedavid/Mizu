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
    struct MeshInfo
    {
        uint64_t mesh_idx;
        uint64_t material_idx;
    };

    static std::optional<AssimpLoader> load(std::filesystem::path path);

    const std::vector<std::shared_ptr<Mesh>>& get_meshes() const { return m_meshes; }
    const std::vector<std::shared_ptr<Material>>& get_materials() const { return m_materials; }
    const std::vector<MeshInfo>& get_meshes_info() const { return m_meshes_info; }

  private:
    std::filesystem::path m_path;
    std::filesystem::path m_container_folder;

    std::vector<std::shared_ptr<Mesh>> m_meshes;
    std::vector<std::shared_ptr<Material>> m_materials;
    std::vector<MeshInfo> m_meshes_info;

    AssimpLoader() = default;
    bool load_internal(std::filesystem::path path);
};

} // namespace Mizu