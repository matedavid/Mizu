#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "base/containers/inplace_vector.h"
#include "base/math/aabb.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "mizu_asset_module.h"

namespace Mizu
{

struct MeshMetadata
{
    uint64_t vertex_count = 0;
    uint64_t index_count = 0;

    IndexBufferFormat index_format = IndexBufferFormat::UInt32;

    uint64_t vertex_data_offset = 0;
    uint64_t index_data_offset = 0;

    AABB bounding_box{};

    inline uint64_t get_vertex_data_size_bytes() const { return vertex_count * sizeof(MeshAssetVertex); }

    inline uint64_t get_index_element_size_bytes() const
    {
        switch (index_format)
        {
        case IndexBufferFormat::UInt16:
            return sizeof(uint16_t);
        case IndexBufferFormat::UInt32:
            return sizeof(uint32_t);
        }
    }

    inline uint64_t get_index_data_size_bytes() const { return index_count * get_index_element_size_bytes(); }

    inline uint64_t get_total_size_bytes() const
    {
        if (index_count > 0)
            return index_data_offset + get_index_data_size_bytes();

        return vertex_data_offset + get_vertex_data_size_bytes();
    }

    inline uint64_t get_vertex_alignment_bytes() const { return alignof(MeshAssetVertex); }
    inline uint64_t get_index_alignment_bytes() const { return get_index_element_size_bytes(); }

    inline uint64_t get_total_alignment_bytes() const
    {
        const uint64_t vertex_alignment = alignof(MeshAssetVertex);
        const uint64_t index_alignment = get_index_element_size_bytes();
        return std::max(vertex_alignment, index_alignment);
    }
};

constexpr size_t MESH_METADATA_SIZE = sizeof(uint64_t) * 4 + sizeof(uint32_t) + sizeof(float) * 6;
static_assert(sizeof(MeshMetadata) >= MESH_METADATA_SIZE, "MeshMetadata size mismatch");

struct TextureMetadata
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;

    uint64_t num_mips = 0;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;

    inline uint64_t get_total_size_bytes() const
    {
        if (width == 0 || height == 0 || depth == 0)
            return 0;

        return width * height * depth * get_image_format_size(format);
    }
};

constexpr size_t TEXTURE_METADATA_SIZE = sizeof(uint32_t) * 3 + sizeof(uint64_t) + sizeof(uint32_t);
static_assert(sizeof(TextureMetadata) >= TEXTURE_METADATA_SIZE, "TextureMetadata size mismatch");

constexpr uint64_t MAX_TEXTURES_PER_MATERIAL = 16;

struct MaterialMetadata
{
    uint32_t num_textures = 0;
    inplace_vector<TextureAssetHandle, MAX_TEXTURES_PER_MATERIAL> texture_handles{};
};

constexpr size_t MATERIAL_METADATA_SIZE =
    sizeof(uint32_t) + sizeof(inplace_vector<TextureAssetHandle, MAX_TEXTURES_PER_MATERIAL>);
static_assert(sizeof(MaterialMetadata) >= MATERIAL_METADATA_SIZE, "MaterialMetadata size mismatch");

struct PrefabMetadata
{
    uint32_t num_meshes = 0;
};

constexpr size_t PREFAB_METADATA_SIZE = sizeof(uint32_t);
static_assert(sizeof(PrefabMetadata) >= PREFAB_METADATA_SIZE, "PrefabMetadata size mismatch");

// Shared Info:
// - shared   metadata version (uint32_t)
// - specific metadata version (uint32_t)
constexpr size_t METADATA_SHARED_INFO_SIZE = sizeof(uint32_t) * 2;

constexpr size_t TOTAL_MESH_METADATA_SIZE = METADATA_SHARED_INFO_SIZE + MESH_METADATA_SIZE;
constexpr size_t TOTAL_TEXTURE_METADATA_SIZE = METADATA_SHARED_INFO_SIZE + TEXTURE_METADATA_SIZE;
constexpr size_t TOTAL_MATERIAL_METADATA_SIZE = METADATA_SHARED_INFO_SIZE + MATERIAL_METADATA_SIZE;
constexpr size_t TOTAL_PREFAB_METADATA_SIZE = METADATA_SHARED_INFO_SIZE + PREFAB_METADATA_SIZE;

MIZU_ASSET_API void mesh_serialize_metadata(const MeshMetadata& metadata, std::span<uint8_t> destination);
MIZU_ASSET_API std::optional<MeshMetadata> mesh_deserialize_metadata(std::span<const uint8_t> data);

MIZU_ASSET_API void texture_serialize_metadata(const TextureMetadata& metadata, std::span<uint8_t> destination);
MIZU_ASSET_API std::optional<TextureMetadata> texture_deserialize_metadata(std::span<const uint8_t> data);

MIZU_ASSET_API void material_serialize_metadata(const MaterialMetadata& metadata, std::span<uint8_t> destination);
MIZU_ASSET_API std::optional<MaterialMetadata> material_deserialize_metadata(std::span<const uint8_t> data);

MIZU_ASSET_API void prefab_serialize_metadata(const PrefabMetadata& metadata, std::span<uint8_t> destination);
MIZU_ASSET_API std::optional<PrefabMetadata> prefab_deserialize_metadata(std::span<const uint8_t> data);

} // namespace Mizu