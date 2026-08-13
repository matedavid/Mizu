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

template <typename T>
static void write_value(uint8_t*& cursor, const T& value)
{
    static_assert(std::is_trivially_copyable_v<T>, "Can only serialize trivially copyable types");

    std::memcpy(cursor, &value, sizeof(T));
    cursor = std::next(cursor, sizeof(T));
}

template <typename T>
static T read_value(const uint8_t*& cursor)
{
    static_assert(std::is_trivially_copyable_v<T>, "Can only deserialize trivially copyable types");

    T value{};
    std::memcpy(&value, cursor, sizeof(T));
    cursor = std::next(cursor, sizeof(T));

    return value;
}

void mesh_serialize_metadata(const MeshMetadata& metadata, std::span<uint8_t> destination)
{
    if (destination.size() < MESH_METADATA_SIZE)
    {
        MIZU_ASSERT(
            false,
            "Destination is too small to serialize MeshMetadata ({} < {})",
            destination.size(),
            MESH_METADATA_SIZE);
        return;
    }

    uint8_t* shared_info_data = destination.data();
    write_value<uint32_t>(shared_info_data, METADATA_VERSION);
    write_value<uint32_t>(shared_info_data, MESH_METADATA_VERSION);

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint64_t>(metadata_data, metadata.vertex_count);
    write_value<uint64_t>(metadata_data, metadata.index_count);
    write_value<uint32_t>(metadata_data, static_cast<uint32_t>(metadata.index_format));
    write_value<uint64_t>(metadata_data, metadata.vertex_data_offset);
    write_value<uint64_t>(metadata_data, metadata.index_data_offset);
    write_value<glm::vec3>(metadata_data, metadata.bounding_box.min());
    write_value<glm::vec3>(metadata_data, metadata.bounding_box.max());
}

std::optional<MeshMetadata> mesh_deserialize_metadata(std::span<const uint8_t> data)
{
    if (data.size() < MESH_METADATA_SIZE)
    {
        MIZU_LOG_ERROR("Data is too small to deserialize MeshMetadata ({} < {})", data.size(), MESH_METADATA_SIZE);
        return std::nullopt;
    }

    const uint8_t* shared_info_data = data.data();

    const uint32_t version = read_value<uint32_t>(shared_info_data);
    if (version != METADATA_VERSION)
    {
        MIZU_LOG_ERROR("Unsupported Metadata version, expected {} but got {}", METADATA_VERSION, version);
        return std::nullopt;
    }

    const uint32_t mesh_version = read_value<uint32_t>(shared_info_data);
    if (mesh_version != MESH_METADATA_VERSION)
    {
        MIZU_LOG_ERROR(
            "Unsupported Mesh Metadata version, expected {} but got {}", MESH_METADATA_VERSION, mesh_version);
        return std::nullopt;
    }

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    MeshMetadata metadata{};

    metadata.vertex_count = read_value<uint64_t>(metadata_data);
    metadata.index_count = read_value<uint64_t>(metadata_data);

    const IndexBufferFormat index_format = static_cast<IndexBufferFormat>(read_value<uint32_t>(metadata_data));
    if (!meta::enum_traits<IndexBufferFormat>::contains(index_format))
    {
        MIZU_LOG_ERROR("Invalid IndexBufferFormat in MeshMetadata: {}", static_cast<uint32_t>(index_format));
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

void texture_serialize_metadata(const TextureMetadata& metadata, std::span<uint8_t> destination)
{
    if (destination.size() < TEXTURE_METADATA_SIZE)
    {
        MIZU_ASSERT(
            false,
            "Destination is too small to serialize TextureMetadata ({} < {})",
            destination.size(),
            TEXTURE_METADATA_SIZE);
        return;
    }

    uint8_t* shared_info_data = destination.data();
    write_value<uint32_t>(shared_info_data, METADATA_VERSION);
    write_value<uint32_t>(shared_info_data, TEXTURE_METADATA_VERSION);

    uint8_t* metadata_data = std::next(destination.data(), METADATA_SHARED_INFO_SIZE);

    write_value<uint32_t>(metadata_data, metadata.width);
    write_value<uint32_t>(metadata_data, metadata.height);
    write_value<uint32_t>(metadata_data, metadata.depth);
    write_value<uint64_t>(metadata_data, metadata.num_mips);
    write_value<uint32_t>(metadata_data, static_cast<uint32_t>(metadata.format));
}

std::optional<TextureMetadata> texture_deserialize_metadata(std::span<const uint8_t> data)
{
    if (data.size() < TEXTURE_METADATA_SIZE)
    {
        MIZU_LOG_ERROR(
            "Data is too small to deserialize TextureMetadata ({} < {})", data.size(), TEXTURE_METADATA_SIZE);
        return std::nullopt;
    }

    const uint8_t* shared_info_data = data.data();

    const uint32_t version = read_value<uint32_t>(shared_info_data);
    if (version != METADATA_VERSION)
    {
        MIZU_LOG_ERROR("Unsupported Metadata version, expected {} but got {}", METADATA_VERSION, version);
        return std::nullopt;
    }

    const uint32_t texture_version = read_value<uint32_t>(shared_info_data);
    if (texture_version != TEXTURE_METADATA_VERSION)
    {
        MIZU_LOG_ERROR(
            "Unsupported Texture Metadata version, expected {} but got {}", TEXTURE_METADATA_VERSION, texture_version);
        return std::nullopt;
    }

    const uint8_t* metadata_data = std::next(data.data(), METADATA_SHARED_INFO_SIZE);

    TextureMetadata metadata{};

    metadata.width = read_value<uint32_t>(metadata_data);
    metadata.height = read_value<uint32_t>(metadata_data);
    metadata.depth = read_value<uint32_t>(metadata_data);
    metadata.num_mips = read_value<uint64_t>(metadata_data);

    const ImageFormat format = static_cast<ImageFormat>(read_value<uint32_t>(metadata_data));
    if (!meta::enum_traits<ImageFormat>::contains(format))
    {
        MIZU_LOG_ERROR("Invalid ImageFormat in TextureMetadata: {}", static_cast<uint32_t>(format));
        return std::nullopt;
    }

    metadata.format = format;

    return metadata;
}

void material_serialize_metadata(const MaterialMetadata& metadata, std::span<uint8_t> destination)
{
    (void)metadata;
    (void)destination;

    MIZU_UNREACHABLE("Not implemented");
}

std::optional<MaterialMetadata> material_deserialize_metadata(std::span<const uint8_t> data)
{
    (void)data;

    MIZU_UNREACHABLE("Not implemented");

    return std::nullopt;
}

} // namespace Mizu