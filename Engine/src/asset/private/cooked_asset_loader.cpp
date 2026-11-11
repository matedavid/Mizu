#include "asset/cooked_asset_loader.h"

#include <fstream>
#include <span>
#include <vector>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

namespace Mizu
{

namespace
{

template <typename RecordT>
using record_metadata_type = decltype(RecordT::metadata);

template <typename MetadataT, std::optional<MetadataT> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
std::optional<MetadataT> read_metadata_from_stream(std::ifstream& file, const AssetLocation& location)
{
    std::vector<uint8_t> buffer(ASSET_FILE_HEADER_SIZE);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size())))
    {
        MIZU_LOG_ERROR("Failed to read file header: {}", location.path.string());
        return std::nullopt;
    }

    const std::optional<AssetFileHeader> header = asset_parse_file_header(buffer);
    if (!header.has_value())
    {
        MIZU_LOG_ERROR("Invalid file header: {}", location.path.string());
        return std::nullopt;
    }

    buffer.resize(ASSET_FILE_HEADER_SIZE + header->metadata_size);
    if (!file.read(
            reinterpret_cast<char*>(buffer.data() + ASSET_FILE_HEADER_SIZE),
            static_cast<std::streamsize>(header->metadata_size)))
    {
        MIZU_LOG_ERROR("Failed to read file metadata: {}", location.path.string());
        return std::nullopt;
    }

    std::optional<MetadataT> metadata = DeserializeMetadataFunc(buffer);
    if (!metadata.has_value())
    {
        MIZU_LOG_ERROR("Failed to deserialize metadata for file: {}", location.path.string());
        return std::nullopt;
    }

    return metadata;
}

template <typename MetadataT, std::optional<MetadataT> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
std::optional<MetadataT> read_metadata_from_location(const AssetLocation& location)
{
    std::ifstream file(location.path, std::ios::binary);
    if (!file.is_open())
    {
        MIZU_LOG_ERROR("Failed to open file: {}", location.path.string());
        return std::nullopt;
    }

    return read_metadata_from_stream<MetadataT, DeserializeMetadataFunc>(file, location);
}

template <typename MetadataT, std::optional<MetadataT> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
bool load_payload_from_location(const AssetLocation& location, std::span<uint8_t> destination)
{
    std::ifstream file(location.path, std::ios::binary);
    if (!file.is_open())
    {
        MIZU_LOG_ERROR("Failed to open file: {}", location.path.string());
        return false;
    }

    const std::optional<MetadataT> metadata =
        read_metadata_from_stream<MetadataT, DeserializeMetadataFunc>(file, location);
    if (!metadata.has_value())
        return false;

    // The payload starts right after the metadata, which is where the stream is positioned now.
    const size_t total_size = metadata->get_total_size_bytes();
    if (total_size == 0)
        return true;

    if (destination.size() < total_size)
    {
        MIZU_LOG_ERROR(
            "Destination span too small for payload of file: {} ({} < {})",
            location.path.string(),
            destination.size(),
            total_size);
        return false;
    }

    if (!file.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(total_size)))
    {
        MIZU_LOG_ERROR("Failed to load payload for file: {}", location.path.string());
        return false;
    }

    return true;
}

template <
    typename RecordT,
    typename AssetHandleT,
    std::optional<record_metadata_type<RecordT>> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
std::optional<RecordT> get_record_internal(const AssetHandleT& handle, const AssetRegistry& registry)
{
    using MetadataT = record_metadata_type<RecordT>;

    const std::optional<AssetLocation> location = registry.resolve(handle);
    if (!location.has_value())
    {
        MIZU_ASSERT(false, "Failed to resolve asset handle: {}", handle.get_id());
        return std::nullopt;
    }

    const std::optional<MetadataT> metadata =
        read_metadata_from_location<MetadataT, DeserializeMetadataFunc>(*location);
    if (!metadata.has_value())
        return std::nullopt;

    return RecordT{
        .handle = handle,
        .metadata = *metadata,
    };
}

template <
    typename MetadataT,
    typename HandleT,
    std::optional<MetadataT> (*DeserializeMetadataFunc)(std::span<const uint8_t>)>
bool load_payload_internal(const HandleT& handle, const AssetRegistry& registry, std::span<uint8_t> destination)
{
    const std::optional<AssetLocation> location = registry.resolve(handle);
    if (!location.has_value())
    {
        MIZU_ASSERT(false, "Failed to resolve asset handle: {}", handle.get_id());
        return false;
    }

    return load_payload_from_location<MetadataT, DeserializeMetadataFunc>(*location, destination);
}

} // namespace

CookedAssetLoader::CookedAssetLoader(const AssetRegistry& registry) : m_registry(registry) {}

std::optional<MeshAssetRecord> CookedAssetLoader::get_mesh_record(const MeshAssetHandle& handle)
{
    return get_record_internal<MeshAssetRecord, MeshAssetHandle, mesh_deserialize_metadata>(handle, m_registry);
}

std::optional<TextureAssetRecord> CookedAssetLoader::get_texture_record(const TextureAssetHandle& handle)
{
    return get_record_internal<TextureAssetRecord, TextureAssetHandle, texture_deserialize_metadata>(
        handle, m_registry);
}

std::optional<MaterialAssetRecord> CookedAssetLoader::get_material_record(const MaterialAssetHandle& handle)
{
    return get_record_internal<MaterialAssetRecord, MaterialAssetHandle, material_deserialize_metadata>(
        handle, m_registry);
}

std::optional<PrefabAssetRecord> CookedAssetLoader::get_prefab_record(const PrefabAssetHandle& handle)
{
    return get_record_internal<PrefabAssetRecord, PrefabAssetHandle, prefab_deserialize_metadata>(handle, m_registry);
}

std::optional<ShaderAssetRecord> CookedAssetLoader::get_shader_record(
    const ShaderAssetHandle& handle,
    ShaderBytecodeTarget target)
{
    const std::optional<AssetLocation> location = m_registry.resolve(handle, target);
    if (!location.has_value())
    {
        MIZU_ASSERT(
            false,
            "Failed to resolve shader handle: {} (bytecode target: {})",
            handle.get_id(),
            meta::enum_name(target));
        return std::nullopt;
    }

    const std::optional<ShaderAssetMetadata> metadata =
        read_metadata_from_location<ShaderAssetMetadata, shader_deserialize_metadata>(*location);
    if (!metadata.has_value())
        return std::nullopt;

    return ShaderAssetRecord{
        .handle = handle,
        .metadata = *metadata,
    };
}

bool CookedAssetLoader::load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<MeshAssetMetadata, MeshAssetHandle, mesh_deserialize_metadata>(
        handle, m_registry, destination);
}

bool CookedAssetLoader::load_mesh_payload_lods(
    const MeshAssetHandle& handle,
    uint8_t first_lod,
    uint8_t lod_count,
    std::span<uint8_t> destination)
{
    const std::optional<AssetLocation> location = m_registry.resolve(handle);
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

    const std::optional<MeshAssetMetadata> metadata =
        read_metadata_from_stream<MeshAssetMetadata, mesh_deserialize_metadata>(file, *location);
    if (!metadata.has_value())
        return false;

    const uint8_t num_lods = metadata->get_num_lods();
    if (lod_count == 0 || first_lod + lod_count > num_lods)
    {
        MIZU_LOG_ERROR(
            "Invalid first lod or lod count first_lod={}, lod_count={}, num_lods={}", first_lod, lod_count, num_lods);
        return false;
    }

    const uint64_t offset = metadata->get_lod_range_file_offset(first_lod, lod_count);

    const uint64_t total_size = metadata->get_lod_range_file_size_bytes(first_lod, lod_count);
    if (total_size == 0)
        return true;

    if (destination.size() < total_size)
    {
        MIZU_LOG_ERROR(
            "Destination span too small for payload of file: {} ({} < {})",
            location->path.string(),
            destination.size(),
            total_size);
        return false;
    }

    file.ignore(static_cast<std::streamsize>(offset));

    if (!file.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(total_size)))
    {
        MIZU_LOG_ERROR("Failed to load payload for file: {}", location->path.string());
        return false;
    }

    return true;
}

bool CookedAssetLoader::load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<TextureAssetMetadata, TextureAssetHandle, texture_deserialize_metadata>(
        handle, m_registry, destination);
}

bool CookedAssetLoader::load_texture_payload_mips(
    const TextureAssetHandle& handle,
    uint32_t first_mip,
    uint32_t mip_count,
    std::span<uint8_t> destination)
{
    const std::optional<AssetLocation> location = m_registry.resolve(handle);
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

    const std::optional<TextureAssetMetadata> metadata =
        read_metadata_from_stream<TextureAssetMetadata, texture_deserialize_metadata>(file, *location);
    if (!metadata.has_value())
        return false;

    if (first_mip >= metadata->num_mips || first_mip + mip_count > metadata->num_mips)
    {
        MIZU_LOG_ERROR(
            "Invalid first mip or mip count first_mip={}, mip_count={}, num_mips={}",
            first_mip,
            mip_count,
            metadata->num_mips);
        return false;
    }

    const uint64_t offset = metadata->get_mip_offset(first_mip);

    const uint64_t total_size = metadata->get_mip_range_size_bytes(first_mip, mip_count);
    if (total_size == 0)
        return true;

    if (destination.size() < total_size)
    {
        MIZU_LOG_ERROR(
            "Destination span too small for payload of file: {} ({} < {})",
            location->path.string(),
            destination.size(),
            total_size);
        return false;
    }

    file.ignore(static_cast<std::streamsize>(offset));

    if (!file.read(reinterpret_cast<char*>(destination.data()), static_cast<std::streamsize>(total_size)))
    {
        MIZU_LOG_ERROR("Failed to load payload for file: {}", location->path.string());
        return false;
    }

    return true;
}

bool CookedAssetLoader::load_prefab_payload(const PrefabAssetHandle& handle, std::span<uint8_t> destination)
{
    return load_payload_internal<PrefabAssetMetadata, PrefabAssetHandle, prefab_deserialize_metadata>(
        handle, m_registry, destination);
}

bool CookedAssetLoader::load_shader_payload(
    const ShaderAssetHandle& handle,
    ShaderBytecodeTarget target,
    std::span<uint8_t> destination)
{
    const std::optional<AssetLocation> location = m_registry.resolve(handle, target);
    if (!location.has_value())
    {
        MIZU_ASSERT(
            false,
            "Failed to resolve shader handle: {} (bytecode target: {})",
            handle.get_id(),
            static_cast<uint32_t>(target));
        return false;
    }

    return load_payload_from_location<ShaderAssetMetadata, shader_deserialize_metadata>(*location, destination);
}

} // namespace Mizu