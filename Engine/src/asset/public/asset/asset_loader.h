#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "asset/asset_handle.h"

namespace Mizu
{

struct AssetPayloadInfo
{
    uint64_t size_bytes = 0;
    uint64_t alignment = alignof(std::max_align_t);
};

struct MeshAssetRecord
{
    MeshAssetHandle handle{};
    AssetPayloadInfo payload{};
};

struct TextureAssetRecord
{
    TextureAssetHandle handle{};
    AssetPayloadInfo payload{};
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