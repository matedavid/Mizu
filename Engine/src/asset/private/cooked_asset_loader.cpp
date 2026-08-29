#include "asset/cooked_asset_loader.h"

#include <fstream>
#include <span>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"

namespace Mizu
{

CookedAssetLoader::CookedAssetLoader(const AssetRegistry& registry) : m_registry(registry) {}

CookedAssetLoader::~CookedAssetLoader() {}

template <typename RecordT>
using record_metadata_type = decltype(RecordT::metadata);

template <
    typename RecordT,
    typename AssetHandleT,
    std::optional<record_metadata_type<RecordT>> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
static std::optional<RecordT> get_record_internal(
    const AssetHandleT& handle,
    size_t metadata_size,
    const AssetRegistry& registry)
{
    using MetadataT = record_metadata_type<RecordT>;

    const std::optional<AssetLocation> location = registry.resolve(handle);
    if (!location.has_value())
    {
        MIZU_ASSERT(false, "Failed to resolve asset handle: {}", handle.get_id());
        return std::nullopt;
    }

    std::ifstream file(location->path, std::ios::binary);
    if (!file.is_open())
    {
        MIZU_LOG_ERROR("Failed to open file: {}", location->path.string());
        return std::nullopt;
    }

    std::vector<uint8_t> buffer(metadata_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(metadata_size)))
    {
        MIZU_LOG_ERROR("Failed to read file: {}", location->path.string());
        return std::nullopt;
    }

    const std::optional<MetadataT> metadata = DeserializeMetadataFunc(buffer);
    if (!metadata.has_value())
    {
        MIZU_LOG_ERROR("Failed to deserialize metadata for file: {}", location->path.string());
        return std::nullopt;
    }

    return RecordT{
        .handle = handle,
        .metadata = *metadata,
    };
}

std::optional<MeshAssetRecord> CookedAssetLoader::get_mesh_record(const MeshAssetHandle& handle)
{
    return get_record_internal<MeshAssetRecord, MeshAssetHandle, mesh_deserialize_metadata>(
        handle, TOTAL_MESH_METADATA_SIZE, m_registry);
}

std::optional<TextureAssetRecord> CookedAssetLoader::get_texture_record(const TextureAssetHandle& handle)
{
    return get_record_internal<TextureAssetRecord, TextureAssetHandle, texture_deserialize_metadata>(
        handle, TOTAL_TEXTURE_METADATA_SIZE, m_registry);
}

std::optional<MaterialAssetRecord> CookedAssetLoader::get_material_record(const MaterialAssetHandle& handle)
{
    return get_record_internal<MaterialAssetRecord, MaterialAssetHandle, material_deserialize_metadata>(
        handle, TOTAL_MATERIAL_METADATA_SIZE, m_registry);
}

std::optional<PrefabAssetRecord> CookedAssetLoader::get_prefab_record(const PrefabAssetHandle& handle)
{
    return get_record_internal<PrefabAssetRecord, PrefabAssetHandle, prefab_deserialize_metadata>(
        handle, TOTAL_PREFAB_METADATA_SIZE, m_registry);
}

std::optional<ShaderDeclarationAssetRecord> CookedAssetLoader::get_shader_declaration_record(
    const ShaderDeclarationAssetHandle& handle)
{
    return get_record_internal<
        ShaderDeclarationAssetRecord,
        ShaderDeclarationAssetHandle,
        shader_declaration_deserialize_metadata>(handle, TOTAL_SHADER_DECLARATION_METADATA_SIZE, m_registry);
}

template <
    typename MetadataT,
    typename HandleT,
    std::optional<MetadataT> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
static bool load_payload_internal(
    const HandleT& handle,
    size_t metadata_size,
    const AssetRegistry& registry,
    std::span<uint8_t> destination)
{
    const std::optional<AssetLocation> location = registry.resolve(handle);
    if (!location.has_value())
    {
        MIZU_ASSERT(false, "Failed to resolve asset handle: {}", handle.get_id());
        return false;
    }

    std::ifstream file(location->path, std::ios::binary);
    if (!file.is_open())
    {
        MIZU_LOG_ERROR("Failed to open file: {}", location->path.string());
        return false;
    }

    std::vector<uint8_t> buffer(metadata_size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(metadata_size)))
    {
        MIZU_LOG_ERROR("Failed to read file metadata: {}", location->path.string());
        return false;
    }

    const std::optional<MetadataT> metadata = DeserializeMetadataFunc(buffer);
    if (!metadata.has_value())
    {
        MIZU_LOG_ERROR("Failed to deserialize metadata for file: {}", location->path.string());
        return false;
    }

    const size_t total_size = metadata->get_total_size_bytes();
    if (total_size == 0)
        return true;

    if (destination.size() < total_size)
    {
        MIZU_LOG_ERROR("Destination span too small for payload handle: {}", handle.get_id());
        return false;
    }

    if (!file.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(total_size)))
    {
        MIZU_LOG_ERROR("Failed to load payload for handle: {}", handle.get_id());
        return false;
    }

    return true;
}

bool CookedAssetLoader::load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<MeshAssetMetadata, MeshAssetHandle, mesh_deserialize_metadata>(
        handle, TOTAL_MESH_METADATA_SIZE, m_registry, destination);
}

bool CookedAssetLoader::load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<TextureAssetMetadata, TextureAssetHandle, texture_deserialize_metadata>(
        handle, TOTAL_TEXTURE_METADATA_SIZE, m_registry, destination);
}

bool CookedAssetLoader::load_prefab_payload(const PrefabAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<PrefabAssetMetadata, PrefabAssetHandle, prefab_deserialize_metadata>(
        handle, TOTAL_PREFAB_METADATA_SIZE, m_registry, destination);
}

bool CookedAssetLoader::load_shader_declaration(
    const ShaderDeclarationAssetHandle& handle,
    std::span<uint8_t> destination)
{
    return load_payload_internal<
        ShaderDeclarationAssetMetadata,
        ShaderDeclarationAssetHandle,
        shader_declaration_deserialize_metadata>(
        handle, TOTAL_SHADER_DECLARATION_METADATA_SIZE, m_registry, destination);
}

} // namespace Mizu