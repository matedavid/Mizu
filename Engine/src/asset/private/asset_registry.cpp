#include "asset/asset_registry.h"

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

namespace Mizu
{

AssetRegistry::AssetRegistry(AssetRegistryDescription desc) : m_description(std::move(desc)) {}

MeshAssetHandle AssetRegistry::get_mesh_handle(std::string_view virtual_path)
{
    return get_handle_internal<MeshAssetHandle, AssetType::Mesh, get_mesh_asset_id>(virtual_path);
}

TextureAssetHandle AssetRegistry::get_texture_handle(std::string_view virtual_path)
{
    return get_handle_internal<TextureAssetHandle, AssetType::Texture, get_texture_asset_id>(virtual_path);
}

MaterialAssetHandle AssetRegistry::get_material_handle(std::string_view virtual_path)
{
    return get_handle_internal<MaterialAssetHandle, AssetType::Material, get_material_asset_id>(virtual_path);
}

PrefabAssetHandle AssetRegistry::get_prefab_handle(std::string_view virtual_path)
{
    return get_handle_internal<PrefabAssetHandle, AssetType::Prefab, get_prefab_asset_id>(virtual_path);
}

ShaderDeclarationAssetHandle AssetRegistry::get_shader_declaration_asset_handle(std::string_view virtual_path)
{
    return get_handle_internal<
        ShaderDeclarationAssetHandle,
        AssetType::ShaderDeclaration,
        get_shader_declaration_asset_id>(virtual_path);
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

template <typename HandleT, AssetType Type, AssetHandleId (*GetAssetIdFunc)(std::string_view)>
HandleT AssetRegistry::get_handle_internal(std::string_view virtual_path)
{
    const AssetHandleId asset_id = GetAssetIdFunc(virtual_path);

    const auto entry_it = m_registry.find(asset_id);

    if (entry_it == m_registry.end())
    {
        const AssetEntry entry{
            .asset_type = Type,
            .virtual_path = std::string{virtual_path},
        };

        const auto& [_, inserted] = m_registry.try_emplace(asset_id, entry);
        if (!inserted)
        {
            MIZU_LOG_ERROR("Asset with id '{}' already exists in the registry", asset_id);
            return HandleT{};
        }
    }

    return HandleT{asset_id};
}

std::optional<AssetLocation> AssetRegistry::resolve(const MeshAssetHandle& handle) const
{
    return resolve_internal<MeshAssetHandle, AssetType::Mesh>(handle);
}

std::optional<AssetLocation> AssetRegistry::resolve(const TextureAssetHandle& handle) const
{
    return resolve_internal<TextureAssetHandle, AssetType::Texture>(handle);
}

std::optional<AssetLocation> AssetRegistry::resolve(const MaterialAssetHandle& handle) const
{
    return resolve_internal<MaterialAssetHandle, AssetType::Material>(handle);
}

std::optional<AssetLocation> AssetRegistry::resolve(const PrefabAssetHandle& handle) const
{
    return resolve_internal<PrefabAssetHandle, AssetType::Prefab>(handle);
}

std::optional<AssetLocation> AssetRegistry::resolve(const ShaderDeclarationAssetHandle& handle) const
{
    return resolve_internal<ShaderDeclarationAssetHandle, AssetType::ShaderDeclaration>(handle);
}

template <typename HandleT, AssetType Type>
std::optional<AssetLocation> AssetRegistry::resolve_internal(const HandleT& handle) const
{
    if (!handle.is_valid())
    {
        MIZU_LOG_ERROR("Trying to resolve an invalid asset handle");
        return std::nullopt;
    }

    const std::filesystem::path path = m_description.cooked_assets_path / std::to_string(handle.get_id());
    if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
    {
        MIZU_LOG_ERROR(
            "Trying to resolve asset handle '{}', did not find appropriate file: {}", handle.get_id(), path.string());
        return std::nullopt;
    }

    return AssetLocation{
        .path = path,
        .virtual_path = "", // TODO:
    };
}

std::string_view AssetRegistry::get_virtual_path(const MeshAssetHandle& handle) const
{
    return get_virtual_path_internal<MeshAssetHandle, AssetType::Mesh>(handle);
}

std::string_view AssetRegistry::get_virtual_path(const TextureAssetHandle& handle) const
{
    return get_virtual_path_internal<TextureAssetHandle, AssetType::Texture>(handle);
}

std::string_view AssetRegistry::get_virtual_path(const MaterialAssetHandle& handle) const
{
    return get_virtual_path_internal<MaterialAssetHandle, AssetType::Material>(handle);
}

std::string_view AssetRegistry::get_virtual_path(const PrefabAssetHandle& handle) const
{
    return get_virtual_path_internal<PrefabAssetHandle, AssetType::Prefab>(handle);
}

std::string_view AssetRegistry::get_virtual_path(const ShaderDeclarationAssetHandle& handle) const
{
    return get_virtual_path_internal<ShaderDeclarationAssetHandle, AssetType::ShaderDeclaration>(handle);
}

template <typename HandleT, AssetType Type>
std::string_view AssetRegistry::get_virtual_path_internal(const HandleT& handle) const
{
    if (!handle.is_valid())
        return {};

    const auto entry_it = m_registry.find(handle.get_id());
    if (entry_it == m_registry.end())
        return {};

    const AssetEntry& entry = entry_it->second;
    if (entry.asset_type != Type)
        return {};

    // const DevAssetLocation* location = std::get_if<DevAssetLocation>(&entry.location);
    // if (location == nullptr)
    //     return {};

    // return location->virtual_path;

    return {};
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

} // namespace Mizu
