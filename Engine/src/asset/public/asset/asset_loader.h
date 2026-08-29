#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "base/math/aabb.h"
#include "render_core/rhi/command_buffer.h"
#include "render_core/rhi/image_resource.h"

#include "asset/asset.h"
#include "asset/asset_handle.h"
#include "asset/asset_metadata.h"

namespace Mizu
{

struct MeshAssetRecord
{
    MeshAssetHandle handle{};
    MeshAssetMetadata metadata{};
};

struct TextureAssetRecord
{
    TextureAssetHandle handle{};
    TextureAssetMetadata metadata{};
};

struct MaterialAssetRecord
{
    MaterialAssetHandle handle{};
    MaterialAssetMetadata metadata{};
};

struct PrefabAssetRecord
{
    PrefabAssetHandle handle{};
    PrefabAssetMetadata metadata{};
};

struct ShaderDeclarationAssetRecord
{
    ShaderDeclarationAssetHandle handle{};
    ShaderDeclarationAssetMetadata metadata{};
};

class IAssetLoader
{
  public:
    virtual ~IAssetLoader() = default;

    virtual std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) = 0;
    virtual std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) = 0;
    virtual std::optional<MaterialAssetRecord> get_material_record(const MaterialAssetHandle& handle) = 0;
    virtual std::optional<PrefabAssetRecord> get_prefab_record(const PrefabAssetHandle& handle) = 0;
    virtual std::optional<ShaderDeclarationAssetRecord> get_shader_declaration_record(
        const ShaderDeclarationAssetHandle& handle) = 0;

    virtual bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) = 0;
    virtual bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) = 0;
    virtual bool load_prefab_payload(const PrefabAssetHandle& handle, std::span<uint8_t> destination) = 0;
    virtual bool load_shader_declaration(
        const ShaderDeclarationAssetHandle& handle,
        std::span<uint8_t> destination) = 0;
};

} // namespace Mizu