#include "asset/asset_registry.h"

#include <type_traits>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/utils/hash.h"

namespace Mizu
{

//
// DevAssetRegistryBuilder
//

DevAssetRegistryBuilder& DevAssetRegistryBuilder::add_mount_point(std::string name, std::filesystem::path path)
{
    MIZU_ASSERT(!name.empty(), "Asset mount point must have a name");
    MIZU_ASSERT(std::filesystem::exists(path), "Path to asset mount point must exist");

    m_asset_mounts.push_back(AssetMount{
        .path = std::move(path),
        .name = std::move(name),
    });

    return *this;
}

//
// AssetRegistry
//

AssetRegistry::AssetRegistry(const DevAssetRegistryBuilder& builder)
{
    const std::span<const AssetMount> mount_points = builder.get_mount_points();
    MIZU_ASSERT(mount_points.size() > 0, "At least one asset mount point is required to create an AssetRegistry");

    for (const AssetMount& mount_point : mount_points)
    {
        if (!is_valid_directory_path(mount_point.path))
        {
            MIZU_LOG_ERROR("Skipping invalid asset mount point '{}'", mount_point.path.string());
            continue;
        }

        m_mount_points_map.emplace(mount_point.name, mount_point.path);
    }
}

MeshAssetHandle AssetRegistry::get_mesh_handle(std::string_view virtual_path)
{
    return get_handle_internal<MeshAssetHandle, AssetType::Mesh>(virtual_path);
}

TextureAssetHandle AssetRegistry::get_texture_handle(std::string_view virtual_path)
{
    return get_handle_internal<TextureAssetHandle, AssetType::Texture>(virtual_path);
}

template <typename HandleT, AssetType Type>
HandleT AssetRegistry::get_handle_internal(std::string_view virtual_path)
{
    const size_t asset_id = get_asset_id(virtual_path);

    const auto entry_it = m_registry.find(asset_id);
    if (entry_it != m_registry.end() && entry_it->second.asset_type == Type)
    {
        return HandleT{asset_id};
    }

    const std::optional<AssetVirtualPathInfo> virtual_path_info = get_asset_virtual_path_info(virtual_path);

    if (!virtual_path_info.has_value())
    {
        MIZU_LOG_ERROR("Invalid virtual path: '{}'", virtual_path);
        return HandleT{};
    }

    const std::optional<std::filesystem::path> physical_path =
        resolve_virtual_path(virtual_path_info->name, virtual_path_info->virtual_path);

    if (!physical_path.has_value())
    {
        MIZU_LOG_ERROR("Failed to resolve virtual path to physical path: '{}'", virtual_path);
        return HandleT{};
    }

    if (!std::filesystem::exists(*physical_path))
    {
        MIZU_LOG_ERROR("Physical asset path does not exist: '{}'", physical_path->string());
        return HandleT{};
    }

    const std::optional<AssetType> asset_type = get_asset_type_from_path(*physical_path);

    if (!asset_type.has_value())
    {
        MIZU_LOG_ERROR("Failed to get asset type from file: '{}'", physical_path->string());
        return HandleT{};
    }

    if (*asset_type != Type)
    {
        MIZU_LOG_ERROR(
            "Expected asset type does not match the file asset type ({} != {})",
            asset_type_to_string(*asset_type),
            asset_type_to_string(Type));
        return HandleT{};
    }

    const AssetEntry entry{
        .asset_type = *asset_type,
        .location = DevAssetLocation{.physical_path = *physical_path},
    };

    m_registry.emplace(asset_id, entry);

    return HandleT{asset_id};
}

template <typename LocationT>
LocationT AssetRegistry::resolve(const MeshAssetHandle& handle) const
{
    return resolve_internal<LocationT, MeshAssetHandle, AssetType::Mesh>(handle);
}

template <typename LocationT>
LocationT AssetRegistry::resolve(const TextureAssetHandle& handle) const
{
    return resolve_internal<LocationT, TextureAssetHandle, AssetType::Texture>(handle);
}

template <typename LocationT, typename HandleT, AssetType Type>
LocationT AssetRegistry::resolve_internal(const HandleT& handle) const
{
    static_assert(
        std::is_same_v<LocationT, DevAssetLocation> || std::is_same_v<LocationT, CookedAssetLocation>,
        "AssetRegistry::resolve only supports known asset location types");

    if (!handle.is_valid())
    {
        MIZU_LOG_ERROR("Trying to resolve an invalid asset handle");
        return LocationT{};
    }

    const auto entry_it = m_registry.find(handle.get_id());
    if (entry_it == m_registry.end())
    {
        MIZU_LOG_ERROR("Could not find asset with id '{}' on AssetRegistry", handle.get_id());
        return LocationT{};
    }

    const AssetEntry& entry = entry_it->second;
    if (entry.asset_type != Type)
    {
        MIZU_LOG_ERROR(
            "Expected asset type does not match the stored asset type ({} != {})",
            asset_type_to_string(entry.asset_type),
            asset_type_to_string(Type));
        return LocationT{};
    }

    const LocationT* location = std::get_if<LocationT>(&entry.location);
    if (location == nullptr)
    {
        MIZU_LOG_ERROR("Expected asset location does not match the stored asset location");
        return LocationT{};
    }

    return *location;
}

template MIZU_ASSET_API DevAssetLocation AssetRegistry::resolve<DevAssetLocation>(const MeshAssetHandle& handle) const;
template MIZU_ASSET_API CookedAssetLocation AssetRegistry::resolve<CookedAssetLocation>(const MeshAssetHandle& handle) const;
template MIZU_ASSET_API DevAssetLocation AssetRegistry::resolve<DevAssetLocation>(const TextureAssetHandle& handle) const;
template MIZU_ASSET_API CookedAssetLocation AssetRegistry::resolve<CookedAssetLocation>(const TextureAssetHandle& handle) const;

std::optional<AssetRegistry::AssetVirtualPathInfo> AssetRegistry::get_asset_virtual_path_info(
    std::string_view virtual_path) const
{
    const size_t pos = virtual_path.find(":");

    if (pos == std::string_view::npos)
    {
        MIZU_LOG_ERROR("Invalid virtual path format: '{}'", virtual_path);
        return std::nullopt;
    }

    const std::string_view name = virtual_path.substr(0, pos);
    const std::string_view path = virtual_path.substr(pos + 1);

    return AssetVirtualPathInfo{
        .name = name,
        .virtual_path = path,
    };
}

std::optional<std::filesystem::path> AssetRegistry::resolve_virtual_path(
    std::string_view name,
    std::string_view virtual_path) const
{
    // TODO: Prevent conversion to std::string
    const auto it = m_mount_points_map.find(std::string{name});

    if (it == m_mount_points_map.end())
        return std::nullopt;

    return std::filesystem::path{it->second / virtual_path};
}

std::optional<AssetType> AssetRegistry::get_asset_type_from_path(const std::filesystem::path& path) const
{
    const std::filesystem::path ext = path.extension();

    if (ext == ".obj" || ext == ".gltf")
        return AssetType::Mesh;

    if (ext == ".jpg" || ext == ".png")
        return AssetType::Texture;

    return std::nullopt;
}

bool AssetRegistry::is_valid_directory_path(const std::filesystem::path& path) const
{
    if (!std::filesystem::exists(path))
    {
        MIZU_LOG_ERROR("Non existant path: {}", path.string());
        return false;
    }

    if (!std::filesystem::is_directory(path))
    {
        MIZU_LOG_ERROR("Path is not a directory: {}", path.string());
        return false;
    }

    return true;
}

size_t AssetRegistry::get_asset_id(std::string_view virtual_path) const
{
    return hash_compute(virtual_path);
}

} // namespace Mizu
