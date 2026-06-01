#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"

namespace Mizu
{

struct MeshPayload
{
    uint64_t vertex_count = 0;
    uint64_t index_count = 0;

    IndexBufferFormat index_format = IndexBufferFormat::UInt32;

    uint64_t vertex_data_offset = 0;
    uint64_t index_data_offset = 0;

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

    inline uint64_t get_total_alignment_bytes() const
    {
        const uint64_t vertex_alignment = alignof(MeshAssetVertex);
        const uint64_t index_alignment = get_index_element_size_bytes();
        return std::max(vertex_alignment, index_alignment);
    }
};

struct TexturePayload
{
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t depth = 0;

    uint64_t num_mips = 0;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;

    inline uint64_t get_total_size_bytes() const
    {
        if (width == 0 || height == 0 || depth == 0)
            return 0;

        return width * height * depth * get_image_format_size(format);
    }
};

struct MeshAssetRecord
{
    MeshAssetHandle handle{};
    MeshPayload payload{};
};

struct TextureAssetRecord
{
    TextureAssetHandle handle{};
    TexturePayload payload{};
};

struct MaterialAssetRecord
{
    MaterialAssetHandle handle{};
    std::vector<TextureAssetHandle> texture_handles{};
};

class IAssetLoader
{
  public:
    virtual ~IAssetLoader() = default;

    virtual std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) = 0;
    virtual std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) = 0;
    virtual std::optional<MaterialAssetRecord> get_material_record(const MaterialAssetHandle& handle) = 0;

    virtual bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) = 0;
    virtual bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) = 0;
};

} // namespace Mizu