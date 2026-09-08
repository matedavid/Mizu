#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "base/containers/inplace_vector.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "mizu_asset_module.h"

namespace Mizu
{

struct AssetLocation
{
    std::filesystem::path path{};
    std::string virtual_path{};
};

struct AssetRegistryDescription
{
    std::filesystem::path cooked_assets_path{};
};

class MIZU_ASSET_API AssetRegistry
{
  public:
    AssetRegistry(AssetRegistryDescription desc);

    MeshAssetHandle get_mesh_handle(std::string_view virtual_path);
    TextureAssetHandle get_texture_handle(std::string_view virtual_path);
    MaterialAssetHandle get_material_handle(std::string_view virtual_path);
    PrefabAssetHandle get_prefab_handle(std::string_view virtual_path);
    ShaderDeclarationAssetHandle get_shader_declaration_asset_handle(std::string_view virtual_path);

    // TEMPORAL
    TextureAssetHandle get_texture_handle_from_physical_path(const std::filesystem::path& physical_path) const;
    // ========

    std::optional<AssetLocation> resolve(const MeshAssetHandle& handle) const;
    std::optional<AssetLocation> resolve(const TextureAssetHandle& handle) const;
    std::optional<AssetLocation> resolve(const MaterialAssetHandle& handle) const;
    std::optional<AssetLocation> resolve(const PrefabAssetHandle& handle) const;
    std::optional<AssetLocation> resolve(const ShaderDeclarationAssetHandle& handle) const;

    std::string_view get_virtual_path(const MeshAssetHandle& handle) const;
    std::string_view get_virtual_path(const TextureAssetHandle& handle) const;
    std::string_view get_virtual_path(const MaterialAssetHandle& handle) const;
    std::string_view get_virtual_path(const PrefabAssetHandle& handle) const;
    std::string_view get_virtual_path(const ShaderDeclarationAssetHandle& handle) const;

  private:
    struct AssetVirtualPathInfo
    {
        std::string_view name;
        std::string_view virtual_path;
    };

    struct AssetEntry
    {
        AssetType asset_type;
        std::string virtual_path;
    };

    AssetRegistryDescription m_description{};

    std::unordered_map<std::string, std::filesystem::path> m_mount_points_map{};
    std::unordered_map<AssetHandleId, AssetEntry> m_registry{};

    template <typename HandleT, AssetType Type, AssetHandleId (*GetAssetIdFunc)(std::string_view)>
    HandleT get_handle_internal(std::string_view virtual_path);

    template <typename HandleT, AssetType Type>
    std::optional<AssetLocation> resolve_internal(const HandleT& handle) const;

    template <typename HandleT, AssetType Type>
    std::string_view get_virtual_path_internal(const HandleT& handle) const;

    // TEMPORAL
    std::optional<std::string> get_virtual_path_from_physical_path(const std::filesystem::path& physical_path) const;
    // ========
};

} // namespace Mizu
