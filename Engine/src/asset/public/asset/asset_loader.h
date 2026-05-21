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

struct AssetPayloadLayout
{
    uint64_t size_bytes = 0;
    uint64_t alignment = 1;
};

struct MeshPayloadLayout : AssetPayloadLayout
{
    uint64_t vertex_count = 0;
    uint64_t index_count = 0;

    IndexBufferFormat index_format = IndexBufferFormat::UInt32;

    uint64_t vertex_data_offset = 0;
    uint64_t index_data_offset = 0;

    uint64_t get_vertex_data_size_bytes() const { return vertex_count * sizeof(MeshAssetVertex); }

    uint64_t get_index_element_size_bytes() const
    {
        switch (index_format)
        {
        case IndexBufferFormat::UInt16:
            return sizeof(uint16_t);
        case IndexBufferFormat::UInt32:
            return sizeof(uint32_t);
        }

        return sizeof(uint32_t);
    }

    uint64_t get_index_data_size_bytes() const { return index_count * get_index_element_size_bytes(); }
    uint64_t get_index_data_alignment_bytes() const { return get_index_element_size_bytes(); }
};

struct TexturePayloadLayout : AssetPayloadLayout
{
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t depth = 0;

    uint64_t num_mips = 0;
    ImageFormat format = ImageFormat::R8G8B8A8_UNORM;
};

struct MeshAssetRecord
{
    MeshAssetHandle handle{};
    MeshPayloadLayout payload{};
};

struct TextureAssetRecord
{
    TextureAssetHandle handle{};
    TexturePayloadLayout payload{};
};

class IAssetLoader
{
  public:
    virtual ~IAssetLoader() = default;

    virtual std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) = 0;
    virtual std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) = 0;

    virtual bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) = 0;
    virtual bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) = 0;
};

} // namespace Mizu