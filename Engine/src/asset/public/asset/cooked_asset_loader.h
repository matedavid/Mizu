#pragma once

#include "asset/asset_loader.h"
#include "asset/asset_registry.h"
#include "mizu_asset_module.h"

namespace Mizu
{

class MIZU_ASSET_API CookedAssetLoader : public IAssetLoader
{
  public:
    CookedAssetLoader(const AssetRegistry& registry);
    ~CookedAssetLoader() override;

    std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) override;
    std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) override;
    std::optional<MaterialAssetRecord> get_material_record(const MaterialAssetHandle& handle) override;
    std::optional<PrefabAssetRecord> get_prefab_record(const PrefabAssetHandle& handle) override;
    std::optional<ShaderDeclarationAssetRecord> get_shader_declaration_record(
        const ShaderDeclarationAssetHandle& handle) override;

    bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) override;
    bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) override;
    bool load_prefab_payload(const PrefabAssetHandle& handle, std::span<uint8_t> destination) override;
    bool load_shader_declaration(const ShaderDeclarationAssetHandle& handle, std::span<uint8_t> destination) override;

  private:
    const AssetRegistry& m_registry;
};

} // namespace Mizu