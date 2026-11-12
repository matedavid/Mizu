#include "asset/asset_metadata.h"

#include <concepts>
#include <cstring>
#include <iterator>
#include <span>
#include <type_traits>

#include "base/debug/assert.h"
#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"

namespace Mizu
{

namespace
{

constexpr uint32_t ASSET_FORMAT_VERSION = 2;

enum class ArchiveMode
{
    Size,
    Write,
    Read,
};

template <ArchiveMode Mode>
class BinaryArchive
{
    using Bytes = std::conditional_t<Mode == ArchiveMode::Read, std::span<const uint8_t>, std::span<uint8_t>>;

  public:
    BinaryArchive() = default;
    explicit BinaryArchive(Bytes data) : m_data(data) {}

    template <typename T>
    void field(T& value)
    {
        if constexpr (requires { serialize_fields(*this, value); })
        {
            serialize_fields(*this, value);
        }
        else
        {
            static_assert(
                std::is_trivially_copyable_v<std::remove_const_t<T>>, "Can only serialize trivially copyable types");

            if constexpr (Mode != ArchiveMode::Size)
            {
                if (m_offset + sizeof(T) > m_data.size())
                {
                    m_ok = false;
                    return;
                }

                const auto bytes = m_data.subspan(m_offset, sizeof(T));

                if constexpr (Mode == ArchiveMode::Write)
                    memcpy(bytes.data(), &value, sizeof(T));
                else
                    memcpy(&value, bytes.data(), sizeof(T));
            }

            m_offset += sizeof(T);
        }
    }

    template <typename Container>
    void array(Container& container, uint32_t count)
    {
        if (count > get_capacity(container))
        {
            m_ok = false;
            return;
        }

        if constexpr (Mode == ArchiveMode::Read && requires { container.resize(count); })
            container.resize(count);

        for (uint32_t i = 0; i < count; ++i)
            field(container[i]);
    }

    bool ok() const { return m_ok; }
    uint64_t offset() const { return m_offset; }

  private:
    Bytes m_data{};
    uint64_t m_offset = 0;
    bool m_ok = true;

    template <typename Container>
    static constexpr size_t get_capacity(const Container& container)
    {
        if constexpr (requires { container.capacity(); })
            return container.capacity();
        else
            return std::size(container);
    }
};

using BinarySizeCounter = BinaryArchive<ArchiveMode::Size>;
using BinaryWriter = BinaryArchive<ArchiveMode::Write>;
using BinaryReader = BinaryArchive<ArchiveMode::Read>;

template <typename MetadataT>
struct MetadataTraits;

template <typename T, typename MetadataT>
concept MetadataOf = std::same_as<std::remove_const_t<T>, MetadataT>;

template <>
struct MetadataTraits<MeshAssetMetadata>
{
    static constexpr MetadataType type = MetadataType::Mesh;
    static constexpr uint32_t version = 4;
};

template <typename Archive, MetadataOf<MeshAssetMetadata> T>
void serialize_fields(Archive& ar, T& metadata)
{
    uint8_t num_lods = metadata.get_num_lods();
    ar.field(num_lods);
    ar.array(metadata.lods, num_lods);
    ar.field(metadata.streamable_lods);
    ar.field(metadata.index_format);
    ar.field(metadata.vertex_stride);
    ar.field(metadata.bounding_box.min);
    ar.field(metadata.bounding_box.max);
    ar.field(metadata.texel_factor);
}

bool validate(const MeshAssetMetadata& metadata)
{
    const uint8_t num_lods = metadata.get_num_lods();

    if (num_lods == 0)
    {
        MIZU_LOG_ERROR("mesh must have at least one lod");
        return false;
    }

    if (metadata.streamable_lods >= num_lods)
    {
        MIZU_LOG_ERROR("streamable_lods ({}) must be lower than num_lods ({})", metadata.streamable_lods, num_lods);
        return false;
    }

    if (metadata.vertex_stride == 0)
    {
        MIZU_LOG_ERROR("vertex_stride can't be 0");
        return false;
    }

    if (!meta::enum_traits<IndexBufferFormat>::contains(metadata.index_format))
    {
        MIZU_LOG_ERROR("invalid IndexBufferFormat ({})", static_cast<uint32_t>(metadata.index_format));
        return false;
    }

    return true;
}

template <>
struct MetadataTraits<TextureAssetMetadata>
{
    static constexpr MetadataType type = MetadataType::Texture;
    static constexpr uint32_t version = 4;
};

template <typename Archive, MetadataOf<TextureAssetMetadata> T>
void serialize_fields(Archive& ar, T& metadata)
{
    ar.field(metadata.width);
    ar.field(metadata.height);
    ar.field(metadata.depth);
    ar.field(metadata.num_mips);
    ar.field(metadata.format);
    ar.field(metadata.streamable_mips);
}

bool validate(const TextureAssetMetadata& metadata)
{
    if (!meta::enum_traits<ImageFormat>::contains(metadata.format))
    {
        MIZU_LOG_ERROR("invalid ImageFormat ({})", static_cast<uint32_t>(metadata.format));
        return false;
    }

    if (metadata.width == 0 || metadata.height == 0 || metadata.depth == 0)
    {
        MIZU_LOG_ERROR("dimensions ({}x{}x{}) can't be 0", metadata.width, metadata.height, metadata.depth);
        return false;
    }

    if (metadata.num_mips == 0)
    {
        MIZU_LOG_ERROR("num_mips can't be 0", metadata.num_mips);
        return false;
    }

    if (metadata.streamable_mips >= metadata.num_mips)
    {
        MIZU_LOG_ERROR(
            "streamable_mips ({}) must be lower than num_mips ({})", metadata.streamable_mips, metadata.num_mips);
        return false;
    }

    return true;
}

template <>
struct MetadataTraits<MaterialAssetMetadata>
{
    static constexpr MetadataType type = MetadataType::Material;
    static constexpr uint32_t version = 2;
};

template <typename Archive, MetadataOf<MaterialAssetMetadata> T>
void serialize_fields(Archive& ar, T& metadata)
{
    ar.field(metadata.shader_handle);
    ar.field(metadata.num_textures);
    ar.array(metadata.texture_handles, metadata.num_textures);
}

bool validate(const MaterialAssetMetadata& metadata)
{
    if (!metadata.shader_handle.is_valid())
    {
        MIZU_LOG_ERROR("shader_handle is invalid");
        return false;
    }

    return true;
}

template <>
struct MetadataTraits<PrefabAssetMetadata>
{
    static constexpr MetadataType type = MetadataType::Prefab;
    static constexpr uint32_t version = 1;
};

template <typename Archive, MetadataOf<PrefabAssetMetadata> T>
void serialize_fields(Archive& ar, T& metadata)
{
    ar.field(metadata.num_meshes);
}

bool validate(const PrefabAssetMetadata&)
{
    return true;
}

template <>
struct MetadataTraits<ShaderAssetMetadata>
{
    static constexpr MetadataType type = MetadataType::Shader;
    static constexpr uint32_t version = 2;
};

template <typename Archive, MetadataOf<ShaderAssetMetadata> T>
void serialize_fields(Archive& ar, T& metadata)
{
    ar.field(metadata.shader_type);
    ar.field(metadata.entry_point_length);
    ar.array(metadata.entry_point, metadata.entry_point_length);
    ar.field(metadata.bytecode_size);
    ar.field(metadata.reflection_size);
    ar.field(metadata.bytecode_offset);
    ar.field(metadata.reflection_offset);
}

bool validate(const ShaderAssetMetadata& metadata)
{
    if (metadata.entry_point_length == 0)
    {
        MIZU_LOG_ERROR("entry point can't be empty");
        return false;
    }

    if (metadata.bytecode_size == 0)
    {
        MIZU_LOG_ERROR("bytecode can't be empty");
        return false;
    }

    return true;
}

template <typename MetadataT>
uint64_t get_metadata_fields_size(const MetadataT& metadata)
{
    BinarySizeCounter counter{};
    serialize_fields(counter, metadata);

    return counter.offset();
}

template <typename MetadataT>
uint64_t serialized_metadata_size(const MetadataT& metadata)
{
    return ASSET_FILE_HEADER_SIZE + get_metadata_fields_size(metadata);
}

template <typename MetadataT>
uint64_t serialize_metadata(const MetadataT& metadata, std::span<uint8_t> destination)
{
    const AssetFileHeader header{
        .format_version = ASSET_FORMAT_VERSION,
        .metadata_type = MetadataTraits<MetadataT>::type,
        .metadata_version = MetadataTraits<MetadataT>::version,
        .metadata_size = static_cast<uint32_t>(get_metadata_fields_size(metadata)),
    };

    BinaryWriter writer{destination};

    writer.field(header.format_version);
    writer.field(header.metadata_type);
    writer.field(header.metadata_version);
    writer.field(header.metadata_size);

    serialize_fields(writer, metadata);

    if (!writer.ok())
    {
        MIZU_ASSERT(
            false,
            "Failed to serialize {} metadata, destination is too small ({} < {})",
            meta::enum_name(header.metadata_type),
            destination.size(),
            ASSET_FILE_HEADER_SIZE + header.metadata_size);
        return 0;
    }

    return writer.offset();
}

template <typename MetadataT>
bool validate_header(const AssetFileHeader& header, uint64_t data_size)
{
    constexpr MetadataType expected_type = MetadataTraits<MetadataT>::type;
    constexpr uint32_t expected_version = MetadataTraits<MetadataT>::version;

    if (header.metadata_type != expected_type)
    {
        MIZU_LOG_ERROR(
            "Unexpected metadata type, expected {} but got {}",
            meta::enum_name(expected_type),
            static_cast<uint32_t>(header.metadata_type));
        return false;
    }

    if (header.metadata_version != expected_version)
    {
        MIZU_LOG_ERROR(
            "Unsupported {} metadata version, expected {} but got {}",
            meta::enum_name(expected_type),
            expected_version,
            header.metadata_version);
        return false;
    }

    if (data_size != header.metadata_size)
    {
        MIZU_LOG_ERROR(
            "{} metadata size mismatch, header says {} bytes but got {}",
            meta::enum_name(expected_type),
            header.metadata_size,
            data_size);
        return false;
    }

    return true;
}

template <typename MetadataT>
std::optional<MetadataT> deserialize_metadata(std::span<const uint8_t> data)
{
    const std::optional<AssetFileHeader> header = asset_parse_file_header(data);
    if (!header.has_value())
        return std::nullopt;

    const std::span<const uint8_t> metadata_data = data.subspan(ASSET_FILE_HEADER_SIZE);
    if (!validate_header<MetadataT>(*header, metadata_data.size()))
        return std::nullopt;

    MetadataT metadata{};

    BinaryReader reader{metadata_data};
    serialize_fields(reader, metadata);

    if (!reader.ok() || reader.offset() != metadata_data.size())
    {
        MIZU_LOG_ERROR(
            "Failed to deserialize {} metadata, data is truncated or corrupt",
            meta::enum_name(MetadataTraits<MetadataT>::type));
        return std::nullopt;
    }

    if (!validate(metadata))
        return std::nullopt;

    return metadata;
}

} // namespace

std::optional<AssetFileHeader> asset_parse_file_header(std::span<const uint8_t> data)
{
    AssetFileHeader header{};

    BinaryReader reader{data};
    reader.field(header.format_version);
    reader.field(header.metadata_type);
    reader.field(header.metadata_version);
    reader.field(header.metadata_size);

    if (!reader.ok())
    {
        MIZU_LOG_ERROR(
            "Failed to read asset file header (data.size() = {}, ASSET_FILE_HEADER_SIZE = {})",
            data.size(),
            ASSET_FILE_HEADER_SIZE);
        return std::nullopt;
    }

    if (header.format_version != ASSET_FORMAT_VERSION)
    {
        MIZU_LOG_ERROR(
            "Unsupported asset format version, expected {} but got {}", ASSET_FORMAT_VERSION, header.format_version);
        return std::nullopt;
    }

    return header;
}

#define IMPLEMENT_METADATA_FUNCTIONS(Type, name)                                             \
    uint64_t name##_serialized_metadata_size(const Type& metadata)                           \
    {                                                                                        \
        return serialized_metadata_size(metadata);                                           \
    }                                                                                        \
    uint64_t name##_serialize_metadata(const Type& metadata, std::span<uint8_t> destination) \
    {                                                                                        \
        return serialize_metadata(metadata, destination);                                    \
    }                                                                                        \
    std::optional<Type> name##_deserialize_metadata(std::span<const uint8_t> data)           \
    {                                                                                        \
        return deserialize_metadata<Type>(data);                                             \
    }

IMPLEMENT_METADATA_FUNCTIONS(MeshAssetMetadata, mesh)
IMPLEMENT_METADATA_FUNCTIONS(TextureAssetMetadata, texture)
IMPLEMENT_METADATA_FUNCTIONS(MaterialAssetMetadata, material)
IMPLEMENT_METADATA_FUNCTIONS(PrefabAssetMetadata, prefab)
IMPLEMENT_METADATA_FUNCTIONS(ShaderAssetMetadata, shader)

#undef IMPLEMENT_METADATA_FUNCTIONS

} // namespace Mizu
