#pragma once

#include <unordered_map>

#include "asset/asset_loader.h"
#include "asset/asset_registry.h"
#include "mizu_asset_module.h"

namespace Assimp
{
class Importer;
};
struct aiScene;

namespace Mizu
{

class MIZU_ASSET_API DevAssetLoader : public IAssetLoader
{
  public:
    DevAssetLoader(const AssetRegistry& registry);
    ~DevAssetLoader() override;

    std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) override;
    std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) override;
    std::optional<MaterialAssetRecord> get_material_record(const MaterialAssetHandle& handle) override;

    bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) override;
    bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) override;

  private:
    const AssetRegistry& m_registry;

    // TODO: This cache is very ugly, but until we have an asset manifest file and separate each mesh in a scene into a
    // different file, this prevents needing to load the entire scene for multiple meshes/materials.
    struct AssimpSceneInfo
    {
        Assimp::Importer* importer;
        const aiScene* scene;
    };
    std::unordered_map<std::string, AssimpSceneInfo> m_scene_cache;

    const aiScene* get_or_load_scene(const std::filesystem::path& path);
};

} // namespace Mizu