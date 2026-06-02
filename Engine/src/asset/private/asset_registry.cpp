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

    m_asset_mounts.push_back(
        AssetMount{
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

MeshAssetHandle AssetRegistry::get_mesh_handle(std::string_view virtual_path, uint32_t submesh)
{
    const SpecificMeshAssetInfo specific_info{
        .submesh = submesh,
    };

    return get_handle_internal<MeshAssetHandle, AssetType::Mesh>(virtual_path, specific_info);
}

TextureAssetHandle AssetRegistry::get_texture_handle(std::string_view virtual_path)
{
    const SpecificTextureAssetInfo specific_info{};

    return get_handle_internal<TextureAssetHandle, AssetType::Texture>(virtual_path, specific_info);
}

MaterialAssetHandle AssetRegistry::get_material_handle(std::string_view virtual_path, uint32_t mesh_material)
{
    const SpecificMaterialAssetInfo specific_info{
        .mesh_material = mesh_material,
    };

    return get_handle_internal<MaterialAssetHandle, AssetType::Material>(virtual_path, specific_info);
}

TextureAssetHandle AssetRegistry::get_texture_handle_from_physical_path(
    const std::filesystem::path& physical_path) const
{
    const std::optional<std::string> virtual_path = get_virtual_path_from_physical_path(physical_path);
    if (!virtual_path.has_value())
    {
        MIZU_LOG_ERROR(
            "Failed to resolve physical texture path '{}' to a mounted virtual path", physical_path.string());
        return TextureAssetHandle{};
    }

    return const_cast<AssetRegistry*>(this)->get_texture_handle(*virtual_path);
}

template <typename HandleT, AssetType Type, typename SpecificInfoT>
HandleT AssetRegistry::get_handle_internal(std::string_view virtual_path, SpecificInfoT specific_info)
{
    const size_t asset_id = get_asset_id(virtual_path, specific_info);

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

    const bool matches_requested_type = [&]() {
        // TODO: This is a bit of a hack to allow material assets to be stored as mesh files
        if constexpr (Type == AssetType::Material)
        {
            return *asset_type == AssetType::Mesh;
        }
        else
        {
            return *asset_type == Type;
        }
    }();

    if (!matches_requested_type)
    {
        MIZU_LOG_ERROR(
            "Expected asset type does not match the file asset type ({} != {})",
            asset_type_to_string(*asset_type),
            asset_type_to_string(Type));
        return HandleT{};
    }

    const AssetEntry entry{
        .asset_type = Type,
        .location =
            DevAssetLocation{
                .physical_path = *physical_path,
                .specific_info = specific_info,
            },
    };

    const auto& [_, inserted] = m_registry.try_emplace(asset_id, entry);
    if (!inserted)
    {
        MIZU_LOG_ERROR("Asset with id '{}' already exists in the registry", asset_id);
        return HandleT{};
    }

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

template <typename LocationT>
LocationT AssetRegistry::resolve(const MaterialAssetHandle& handle) const
{
    return resolve_internal<LocationT, MaterialAssetHandle, AssetType::Material>(handle);
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

// clang-format off
template MIZU_ASSET_API DevAssetLocation AssetRegistry::resolve<DevAssetLocation>(const MeshAssetHandle& handle) const;
template MIZU_ASSET_API CookedAssetLocation AssetRegistry::resolve<CookedAssetLocation>(const MeshAssetHandle& handle) const;
template MIZU_ASSET_API DevAssetLocation AssetRegistry::resolve<DevAssetLocation>(const TextureAssetHandle& handle) const;
template MIZU_ASSET_API CookedAssetLocation AssetRegistry::resolve<CookedAssetLocation>(const TextureAssetHandle& handle) const;
template MIZU_ASSET_API DevAssetLocation AssetRegistry::resolve<DevAssetLocation>(const MaterialAssetHandle& handle) const;
template MIZU_ASSET_API CookedAssetLocation AssetRegistry::resolve<CookedAssetLocation>(const MaterialAssetHandle& handle) const;
// clang-format on

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

std::optional<std::string> AssetRegistry::get_virtual_path_from_physical_path(
    const std::filesystem::path& physical_path) const
{
    const auto normalize_path = [](const std::filesystem::path& path) {
        std::error_code error_code;
        const std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path, error_code);
        if (!error_code)
            return canonical_path;

        return path.lexically_normal();
    };

    const std::filesystem::path normalized_physical_path = normalize_path(physical_path);

    for (const auto& [mount_name, mount_path] : m_mount_points_map)
    {
        const std::filesystem::path normalized_mount_path = normalize_path(mount_path);
        const std::filesystem::path relative_path = normalized_physical_path.lexically_relative(normalized_mount_path);

        if (relative_path.empty())
            continue;

        const auto relative_it = relative_path.begin();
        if (relative_it != relative_path.end() && *relative_it == "..")
            continue;

        return std::string{mount_name} + ":" + relative_path.generic_string();
    }

    return std::nullopt;
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

size_t AssetRegistry::get_asset_id(std::string_view virtual_path, const SpecificMeshAssetInfo& specific_info) const
{
    size_t h = 0;

    hash_combine(h, virtual_path);
    hash_combine(h, AssetType::Mesh);

    hash_combine(h, specific_info.submesh);

    return h;
}

size_t AssetRegistry::get_asset_id(std::string_view virtual_path, const SpecificTextureAssetInfo&) const
{
    return hash_compute(virtual_path, AssetType::Texture);
}

size_t AssetRegistry::get_asset_id(std::string_view virtual_path, const SpecificMaterialAssetInfo& specific_info) const
{
    size_t h = 0;

    hash_combine(h, virtual_path);
    hash_combine(h, AssetType::Material);

    hash_combine(h, specific_info.mesh_material);

    return h;
}

} // namespace Mizu
