#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string>

#include "base/containers/inplace_vector.h"
#include "base/debug/assert.h"
#include "base/math/math.h"
#include "base/math/shapes.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"
#include "render_core/rhi/shader.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "mizu_asset_module.h"

namespace Mizu
{

constexpr uint32_t MAX_NUM_MESH_LODS = 10;
static_assert(MAX_NUM_MESH_LODS <= UINT8_MAX);
// Safe alignment so that any vertex component (up to float4) can be read in place regardless of verted_stride.
constexpr uint64_t MESH_FILE_SEGMENT_ALIGNMENT = 16;

struct MeshAssetLod
{
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    float screen_size = 0.0f;
};

struct MeshAssetMetadata
{
    // Payload layout is:
    // [ V(n-1) I(n-1) | V(n-2) I(n-2) | ... | V0 I0 ]
    inplace_vector<MeshAssetLod, MAX_NUM_MESH_LODS> lods{};

    uint8_t streamable_lods = 0;
    IndexBufferFormat index_format = IndexBufferFormat::UInt32;
    uint32_t vertex_stride = 0;

    math::AABB bounding_box{};
    float texel_factor = 0.0f;

    uint8_t get_num_lods() const { return static_cast<uint8_t>(lods.size()); }

    uint64_t get_vertex_stride_bytes() const { return vertex_stride; }

    uint64_t get_index_stride_bytes() const
    {
        switch (index_format)
        {
        case IndexBufferFormat::UInt16:
            return sizeof(uint16_t);
        case IndexBufferFormat::UInt32:
            return sizeof(uint32_t);
        }

        MIZU_UNREACHABLE("Invalid IndexBufferFormat");
        return 0;
    }

    uint32_t get_draw_vertex_count(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return sum_vertex_count(lod);
    }

    uint32_t get_draw_first_index(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return sum_index_count(lod + 1);
    }

    uint32_t get_draw_index_count(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return lods[lod].index_count;
    }

    uint64_t get_vertex_data_size_bytes(uint8_t finest_lod) const
    {
        assert_valid_lod(finest_lod);
        return static_cast<uint64_t>(sum_vertex_count(finest_lod)) * get_vertex_stride_bytes();
    }

    uint64_t get_index_data_size_bytes(uint8_t finest_lod) const
    {
        assert_valid_lod(finest_lod);
        return static_cast<uint64_t>(sum_index_count(finest_lod)) * get_index_stride_bytes();
    }

    uint64_t get_lod_vertex_offset_bytes(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return static_cast<uint64_t>(sum_vertex_count(lod + 1)) * get_vertex_stride_bytes();
    }

    uint64_t get_lod_vertex_size_bytes(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return static_cast<uint64_t>(lods[lod].vertex_count) * get_vertex_stride_bytes();
    }

    uint64_t get_lod_index_offset_bytes(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return static_cast<uint64_t>(sum_index_count(lod + 1)) * get_index_stride_bytes();
    }

    uint64_t get_lod_index_size_bytes(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return static_cast<uint64_t>(lods[lod].index_count) * get_index_stride_bytes();
    }

    uint64_t get_lod_vertex_file_offset(uint8_t lod) const
    {
        assert_valid_lod(lod);
        return sum_file_size_bytes(lod + 1);
    }

    uint64_t get_lod_index_file_offset(uint8_t lod) const
    {
        return get_lod_vertex_file_offset(lod) + get_vertex_segment_size_bytes(lod);
    }

    uint64_t get_lod_range_file_offset(uint8_t first_lod, uint8_t count) const
    {
        assert_valid_lod_range(first_lod, count);
        return sum_file_size_bytes(first_lod + count);
    }

    uint64_t get_lod_range_file_size_bytes(uint8_t first_lod, uint8_t count) const
    {
        assert_valid_lod_range(first_lod, count);
        return sum_file_size_bytes(first_lod) - sum_file_size_bytes(first_lod + count);
    }

    uint64_t get_total_size_bytes() const { return sum_file_size_bytes(0); }

  private:
    void assert_valid_lod(uint8_t lod) const
    {
        MIZU_ASSERT(lod < get_num_lods(), "Invalid lod {} for asset with {} lods", lod, get_num_lods());
    }

    void assert_valid_lod_range(uint8_t first_lod, uint8_t count) const
    {
        MIZU_ASSERT(
            count > 0 && first_lod + count <= get_num_lods(),
            "Invalid lod range [{}, {}) for asset with {} lods",
            first_lod,
            first_lod + count,
            get_num_lods());
    }

    uint32_t sum_vertex_count(uint8_t first_lod) const
    {
        uint32_t count = 0;
        for (uint8_t lod = first_lod; lod < get_num_lods(); ++lod)
            count += lods[lod].vertex_count;
        return count;
    }

    uint32_t sum_index_count(uint8_t first_lod) const
    {
        uint32_t count = 0;
        for (uint8_t lod = first_lod; lod < get_num_lods(); ++lod)
            count += lods[lod].index_count;
        return count;
    }

    uint64_t get_vertex_segment_size_bytes(uint8_t lod) const
    {
        return math::align_up(get_lod_vertex_size_bytes(lod), MESH_FILE_SEGMENT_ALIGNMENT);
    }

    uint64_t get_index_segment_size_bytes(uint8_t lod) const
    {
        return math::align_up(get_lod_index_size_bytes(lod), MESH_FILE_SEGMENT_ALIGNMENT);
    }

    uint64_t sum_file_size_bytes(uint8_t first_lod) const
    {
        uint64_t size = 0;
        for (uint8_t lod = first_lod; lod < get_num_lods(); ++lod)
            size += get_vertex_segment_size_bytes(lod) + get_index_segment_size_bytes(lod);
        return size;
    }
};

struct TextureAssetMetadata
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth = 0;

    uint8_t num_mips = 1;
    ImageFormat format = ImageFormat::R8G8B8A8_SRGB;

    uint8_t streamable_mips = 0;

    uint64_t get_mip_size_bytes(uint8_t mip) const
    {
        MIZU_ASSERT(
            mip < num_mips, "Trying to request mip size in bytes for mip {} on metadata with {} mips", mip, num_mips);

        if (width == 0 || height == 0 || depth == 0)
            return 0;

        const glm::uvec2 mip_size = compute_mip_size(width, height, mip);
        return static_cast<uint64_t>(mip_size.x) * mip_size.y * depth * get_image_format_size(format);
    }

    uint64_t get_mip_offset(uint8_t mip) const
    {
        uint64_t offset = 0;
        for (uint8_t i = 0; i < mip; ++i)
            offset += get_mip_size_bytes(i);

        return offset;
    }

    uint64_t get_mip_range_size_bytes(uint8_t first, uint8_t count) const
    {
        if (width == 0 || height == 0 || depth == 0 || num_mips == 0)
            return 0;

        const uint64_t start = get_mip_offset(first);
        const uint64_t end = get_mip_offset(first + count);

        return end - start;
    }

    uint64_t get_total_size_bytes() const
    {
        if (width == 0 || height == 0 || depth == 0 || num_mips == 0)
            return 0;

        return get_mip_offset(num_mips); // offset "one past the last mip" == sum of all mip sizes
    }
};

constexpr uint32_t MAX_TEXTURES_PER_MATERIAL = 16;

struct MaterialAssetMetadata
{
    ShaderAssetHandle shader_handle;
    uint32_t num_textures = 0;
    inplace_vector<TextureAssetHandle, MAX_TEXTURES_PER_MATERIAL> texture_handles{};
};

struct PrefabMeshInfo
{
    MeshAssetHandle mesh_handle;
    MaterialAssetHandle material_handle;
};

struct PrefabAssetMetadata
{
    uint32_t num_meshes = 0;

    uint64_t get_total_size_bytes() const { return num_meshes * sizeof(PrefabMeshInfo); }
};

struct ShaderAssetMetadata
{
    static constexpr size_t MAX_ENTRY_POINT_LENGTH = 64;

    ShaderType shader_type{};
    uint32_t entry_point_length = 0;
    char entry_point[MAX_ENTRY_POINT_LENGTH]{0};

    uint64_t bytecode_size = 0;
    uint64_t reflection_size = 0;

    uint64_t bytecode_offset = 0;
    uint64_t reflection_offset = 0;

    uint64_t get_total_size_bytes() const
    {
        // Layout is always [ bytecode | reflection ]
        return reflection_offset + reflection_size;
    }

    void set_entry_point(std::string_view entry_point_)
    {
        if (entry_point_.size() > MAX_ENTRY_POINT_LENGTH)
        {
            MIZU_LOG_WARNING(
                "Entry point length exceeds max entry point length, will clamp ({} > {})",
                entry_point_.size(),
                MAX_ENTRY_POINT_LENGTH);
        }

        const size_t length = std::min(entry_point_.size(), MAX_ENTRY_POINT_LENGTH);

        memset(entry_point, 0, MAX_ENTRY_POINT_LENGTH);
        memcpy(entry_point, entry_point_.data(), length);
        entry_point_length = static_cast<uint32_t>(length);
    }

    std::string get_entry_point() const { return std::string{}.assign(entry_point, entry_point_length); }
};

enum class MetadataType : uint32_t
{
    Mesh,
    Texture,
    Material,
    Prefab,
    Shader,
};

struct AssetFileHeader
{
    uint32_t format_version = 0;
    MetadataType metadata_type{};
    uint32_t metadata_version = 0;
    uint32_t metadata_size = 0;
};

constexpr uint64_t ASSET_FILE_HEADER_SIZE = sizeof(uint32_t) * 4;

MIZU_ASSET_API std::optional<AssetFileHeader> asset_parse_file_header(std::span<const uint8_t> data);

#define DEFINE_METADATA_FUNCTIONS(Type, name)                                                                \
    MIZU_ASSET_API uint64_t name##_serialized_metadata_size(const Type& metadata);                           \
    MIZU_ASSET_API uint64_t name##_serialize_metadata(const Type& metadata, std::span<uint8_t> destination); \
    MIZU_ASSET_API std::optional<Type> name##_deserialize_metadata(std::span<const uint8_t> data);

DEFINE_METADATA_FUNCTIONS(MeshAssetMetadata, mesh)
DEFINE_METADATA_FUNCTIONS(TextureAssetMetadata, texture)
DEFINE_METADATA_FUNCTIONS(MaterialAssetMetadata, material)
DEFINE_METADATA_FUNCTIONS(PrefabAssetMetadata, prefab)
DEFINE_METADATA_FUNCTIONS(ShaderAssetMetadata, shader)

#undef DEFINE_METADATA_FUNCTIONS

} // namespace Mizu