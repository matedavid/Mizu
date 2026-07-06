#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#include "base/containers/inplace_vector.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "mizu_asset_module.h"

namespace Mizu
{

struct SpecificMeshAssetInfo
{
    uint32_t submesh = 0;
};

struct SpecificTextureAssetInfo
{
};

struct SpecificMaterialAssetInfo
{
    uint32_t mesh_material = 0;
};

using SpecificAssetInfo = std::variant<SpecificMeshAssetInfo, SpecificTextureAssetInfo, SpecificMaterialAssetInfo>;

struct DevAssetLocation
{
    std::filesystem::path physical_path{};
    std::string virtual_path{};
    SpecificAssetInfo specific_info{};
};

struct CookedAssetLocation
{
    // TODO:
};

class MIZU_ASSET_API DevAssetRegistryBuilder
{
  public:
    DevAssetRegistryBuilder& add_mount_point(std::string name, std::filesystem::path path);

    std::span<const AssetMount> get_mount_points() const { return m_asset_mounts; }

  private:
    inplace_vector<AssetMount, MaxAssetMounts> m_asset_mounts;
};

class MIZU_ASSET_API AssetRegistry
{
  public:
    AssetRegistry(const DevAssetRegistryBuilder& builder);

    MeshAssetHandle get_mesh_handle(std::string_view virtual_path, uint32_t submesh = 0);
    TextureAssetHandle get_texture_handle(std::string_view virtual_path);
    MaterialAssetHandle get_material_handle(std::string_view virtual_path, uint32_t mesh_material = 0);

    // TEMPORAL
    TextureAssetHandle get_texture_handle_from_physical_path(const std::filesystem::path& physical_path) const;
    // ========

    template <typename LocationT>
    LocationT resolve(const MeshAssetHandle& handle) const;
    template <typename LocationT>
    LocationT resolve(const TextureAssetHandle& handle) const;
    template <typename LocationT>
    LocationT resolve(const MaterialAssetHandle& handle) const;

    std::string_view get_virtual_path(const MeshAssetHandle& handle) const;
    std::string_view get_virtual_path(const TextureAssetHandle& handle) const;
    std::string_view get_virtual_path(const MaterialAssetHandle& handle) const;

  private:
    using AssetLocation = std::variant<DevAssetLocation, CookedAssetLocation>;

    struct AssetVirtualPathInfo
    {
        std::string_view name;
        std::string_view virtual_path;
    };

    struct AssetEntry
    {
        AssetType asset_type;
        AssetLocation location;
    };

    std::unordered_map<std::string, std::filesystem::path> m_mount_points_map;
    std::unordered_map<size_t, AssetEntry> m_registry;

    template <typename HandleT, AssetType Type, typename SpecificInfoT>
    HandleT get_handle_internal(std::string_view virtual_path, SpecificInfoT specific_info);

    template <typename LocationT, typename HandleT, AssetType Type>
    LocationT resolve_internal(const HandleT& handle) const;

    template <typename HandleT, AssetType Type>
    std::string_view get_virtual_path_internal(const HandleT& handle) const;

    std::optional<AssetVirtualPathInfo> get_asset_virtual_path_info(std::string_view virtual_path) const;
    std::optional<std::filesystem::path> resolve_virtual_path(std::string_view name, std::string_view virtual_path)
        const;

    // TEMPORAL
    std::optional<std::string> get_virtual_path_from_physical_path(const std::filesystem::path& physical_path) const;
    // ========

    std::optional<AssetType> get_asset_type_from_path(const std::filesystem::path& path) const;
    bool is_valid_directory_path(const std::filesystem::path& path) const;

    size_t get_asset_id(std::string_view virtual_path, const SpecificMeshAssetInfo& specific_info) const;
    size_t get_asset_id(std::string_view virtual_path, const SpecificTextureAssetInfo& specific_info) const;
    size_t get_asset_id(std::string_view virtual_path, const SpecificMaterialAssetInfo& specific_info) const;
};

} // namespace Mizu
