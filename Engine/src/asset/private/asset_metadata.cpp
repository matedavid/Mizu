#include "asset/asset_metadata.h"

#include <cstring>
#include <iterator>
#include <type_traits>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

namespace Mizu
{

static constexpr uint32_t METADATA_VERSION = 1;
static constexpr uint32_t MESH_METADATA_VERSION = 1;
static constexpr uint32_t TEXTURE_METADATA_VERSION = 1;
static constexpr uint32_t MATERIAL_METADATA_VERSION = 1;
static constexpr uint32_t PREFAB_METADATA_VERSION = 1;
static constexpr uint32_t SHADER_DECLARATION_METADATA_VERSION = 1;

template <typename T>
static void write_value(uint8_t*& cursor, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Can only serialize trivially copyable types");

    memcpy(cursor, &value, sizeof(T));
    cursor = std::next(cursor, sizeof(T));
}

template <typename T>
static T read_value(const uint8_t*& cursor)
{
    static_assert(std::is_trivially_copyable_v<T>, "Can only deserialize trivially copyable types");

    T value{};
    memcpy(&value, cursor, sizeof(T));
    cursor = std::next(cursor, sizeof(T));

    return value;
}

static bool serialize_shared_data(std::span<uint8_t> destination, size_t total_metadata_size, uint32_t metadata_version)
{
    if (destination.size() < total_metadata_size)
    {
        MIZU_ASSERT(
            false, "Destination is too small to serialize metadata ({} < {})", destination.size(), total_metadata_size);
        return false;
    }

    uint8_t* shared_info_data = destination.data();
    write_value<uint32_t>(shared_info_data, METADATA_VERSION);
    write_value<uint32_t>(shared_info_data, metadata_version);

    return true;
}

static bool deserialize_shared_data(
    std::span<const uint8_t> data,
    size_t total_metadata_size,
    uint32_t metadata_version)
{
    if (data.size() < total_metadata_size)
    {
        MIZU_LOG_ERROR("Data is too small to deserialize metadata ({} < {})", data.size(), total_metadata_size);
        return false;
    }

    const uint8_t* shared_info_data = data.data();

    const uint32_t version = read_value<uint32_t>(shared_info_data);
    if (version != METADATA_VERSION)
    {
        MIZU_LOG_ERROR("Unsupported metadata version, expected {} but got {}", METADATA_VERSION, version);
        return false;
    }

    const uint32_t specific_version = read_value<uint32_t>(shared_info_data);
    if (specific_version != metadata_version)
    {
        MIZU_LOG_ERROR(
            "Unsupported specific metadata version, expected {} but got {}", metadata_version, specific_version);
        return false;
    }

    return true;
}

void mesh_serialize_metadata(const MeshAssetMetadata& metadata, std::span<uint8_t> destination)
{
    if (!serialize_shared_data(destination, TOTAL_MESH_METADATA_SIZE, MESH_METADATA_VERSION))
        return;

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint64_t>(metadata_data, metadata.vertex_count);
    write_value<uint64_t>(metadata_data, metadata.index_count);
    write_value<uint32_t>(metadata_data, static_cast<uint32_t>(metadata.index_format));
    write_value<uint64_t>(metadata_data, metadata.vertex_data_offset);
    write_value<uint64_t>(metadata_data, metadata.index_data_offset);
    write_value<glm::vec3>(metadata_data, metadata.bounding_box.min());
    write_value<glm::vec3>(metadata_data, metadata.bounding_box.max());
}

std::optional<MeshAssetMetadata> mesh_deserialize_metadata(std::span<const uint8_t> data)
{
    if (!deserialize_shared_data(data, TOTAL_MESH_METADATA_SIZE, MESH_METADATA_VERSION))
        return std::nullopt;

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    MeshAssetMetadata metadata{};

    metadata.vertex_count = read_value<uint64_t>(metadata_data);
    metadata.index_count = read_value<uint64_t>(metadata_data);

    const IndexBufferFormat index_format = static_cast<IndexBufferFormat>(read_value<uint32_t>(metadata_data));
    if (!meta::enum_traits<IndexBufferFormat>::contains(index_format))
    {
        MIZU_LOG_ERROR("Invalid IndexBufferFormat in MeshAssetMetadata: {}", static_cast<uint32_t>(index_format));
        return std::nullopt;
    }

    metadata.index_format = index_format;

    metadata.vertex_data_offset = read_value<uint64_t>(metadata_data);
    metadata.index_data_offset = read_value<uint64_t>(metadata_data);

    const glm::vec3 bounding_box_min = read_value<glm::vec3>(metadata_data);
    const glm::vec3 bounding_box_max = read_value<glm::vec3>(metadata_data);
    metadata.bounding_box = AABB{bounding_box_min, bounding_box_max};

    return metadata;
}

void texture_serialize_metadata(const TextureAssetMetadata& metadata, std::span<uint8_t> destination)
{
    if (!serialize_shared_data(destination, TOTAL_TEXTURE_METADATA_SIZE, TEXTURE_METADATA_VERSION))
        return;

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint32_t>(metadata_data, metadata.width);
    write_value<uint32_t>(metadata_data, metadata.height);
    write_value<uint32_t>(metadata_data, metadata.depth);
    write_value<uint64_t>(metadata_data, metadata.num_mips);
    write_value<uint32_t>(metadata_data, static_cast<uint32_t>(metadata.format));
}

std::optional<TextureAssetMetadata> texture_deserialize_metadata(std::span<const uint8_t> data)
{
    if (!deserialize_shared_data(data, TOTAL_TEXTURE_METADATA_SIZE, TEXTURE_METADATA_VERSION))
        return std::nullopt;

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    TextureAssetMetadata metadata{};

    metadata.width = read_value<uint32_t>(metadata_data);
    metadata.height = read_value<uint32_t>(metadata_data);
    metadata.depth = read_value<uint32_t>(metadata_data);
    metadata.num_mips = read_value<uint64_t>(metadata_data);

    const ImageFormat format = static_cast<ImageFormat>(read_value<uint32_t>(metadata_data));
    if (!meta::enum_traits<ImageFormat>::contains(format))
    {
        MIZU_LOG_ERROR("Invalid ImageFormat in TextureAssetMetadata: {}", static_cast<uint32_t>(format));
        return std::nullopt;
    }

    metadata.format = format;

    return metadata;
}

void material_serialize_metadata(const MaterialAssetMetadata& metadata, std::span<uint8_t> destination)
{
    if (!serialize_shared_data(destination, TOTAL_MATERIAL_METADATA_SIZE, MATERIAL_METADATA_VERSION))
        return;

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint32_t>(metadata_data, metadata.num_textures);

    for (uint32_t i = 0; i < metadata.num_textures; ++i)
    {
        write_value<uint64_t>(metadata_data, metadata.texture_handles[i].get_id());
    }

    for (size_t i = metadata.texture_handles.size(); i < MAX_TEXTURES_PER_MATERIAL; ++i)
    {
        write_value<uint64_t>(metadata_data, 0u);
    }
}

std::optional<MaterialAssetMetadata> material_deserialize_metadata(std::span<const uint8_t> data)
{
    if (!deserialize_shared_data(data, TOTAL_MATERIAL_METADATA_SIZE, MATERIAL_METADATA_VERSION))
        return std::nullopt;

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    MaterialAssetMetadata metadata{};

    metadata.num_textures = read_value<uint32_t>(metadata_data);

    if (metadata.num_textures > MAX_TEXTURES_PER_MATERIAL)
    {
        MIZU_LOG_ERROR(
            "Corrupt material metadata: num_textures ({}) > MAX ({})",
            metadata.num_textures,
            MAX_TEXTURES_PER_MATERIAL);

        return std::nullopt;
    }

    for (uint32_t i = 0; i < metadata.num_textures; ++i)
    {
        const uint64_t handle_id = read_value<uint64_t>(metadata_data);
        metadata.texture_handles.push_back(TextureAssetHandle{handle_id});
    }

    return metadata;
}

void prefab_serialize_metadata(const PrefabAssetMetadata& metadata, std::span<uint8_t> destination)
{
    if (!serialize_shared_data(destination, TOTAL_PREFAB_METADATA_SIZE, PREFAB_METADATA_VERSION))
        return;

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint32_t>(metadata_data, metadata.num_meshes);
}

std::optional<PrefabAssetMetadata> prefab_deserialize_metadata(std::span<const uint8_t> data)
{
    if (!deserialize_shared_data(data, TOTAL_PREFAB_METADATA_SIZE, PREFAB_METADATA_VERSION))
        return std::nullopt;

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    PrefabAssetMetadata metadata{};

    metadata.num_meshes = read_value<uint32_t>(metadata_data);

    return metadata;
}

void shader_declaration_serialize_metadata(
    const ShaderDeclarationAssetMetadata& metadata,
    std::span<uint8_t> destination)
{
    if (!serialize_shared_data(
            destination, TOTAL_SHADER_DECLARATION_METADATA_SIZE, SHADER_DECLARATION_METADATA_VERSION))
        return;

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint64_t>(metadata_data, metadata.bytecode_size);
    write_value<uint64_t>(metadata_data, metadata.reflection_size);
    write_value<uint64_t>(metadata_data, metadata.bytecode_offset);
    write_value<uint64_t>(metadata_data, metadata.reflection_offset);
}

std::optional<ShaderDeclarationAssetMetadata> shader_declaration_deserialize_metadata(std::span<const uint8_t> data)
{
    if (!deserialize_shared_data(data, TOTAL_SHADER_DECLARATION_METADATA_SIZE, SHADER_DECLARATION_METADATA_VERSION))
        return std::nullopt;

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    ShaderDeclarationAssetMetadata metadata{};

    metadata.bytecode_size = read_value<uint64_t>(metadata_data);
    metadata.reflection_size = read_value<uint64_t>(metadata_data);
    metadata.bytecode_offset = read_value<uint64_t>(metadata_data);
    metadata.reflection_offset = read_value<uint64_t>(metadata_data);

    return metadata;
}

} // namespace Mizu