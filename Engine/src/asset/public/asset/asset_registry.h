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

struct DevAssetLocation
{
    std::filesystem::path physical_path;
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

    MeshAssetHandle get_mesh_handle(std::string_view virtual_path);
    TextureAssetHandle get_texture_handle(std::string_view virtual_path);

    template <typename LocationT>
    LocationT resolve(const MeshAssetHandle& handle) const;
    template <typename LocationT>
    LocationT resolve(const TextureAssetHandle& handle) const;

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

    template <typename HandleT, AssetType Type>
    HandleT get_handle_internal(std::string_view virtual_path);

    template <typename LocationT, typename HandleT, AssetType Type>
    LocationT resolve_internal(const HandleT& handle) const;

    std::optional<AssetVirtualPathInfo> get_asset_virtual_path_info(std::string_view virtual_path) const;
    std::optional<std::filesystem::path> resolve_virtual_path(std::string_view name, std::string_view virtual_path)
        const;

    std::optional<AssetType> get_asset_type_from_path(const std::filesystem::path& path) const;
    bool is_valid_directory_path(const std::filesystem::path& path) const;

    size_t get_asset_id(std::string_view virtual_path) const;
};

} // namespace Mizu
