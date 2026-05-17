#pragma once

#include "asset/asset_loader.h"
#include "asset/asset_registry.h"
#include "mizu_asset_module.h"

namespace Mizu
{

class MIZU_ASSET_API DevAssetLoader : public IAssetLoader
{
  public:
    DevAssetLoader(const AssetRegistry& registry);
    ~DevAssetLoader() override = default;

    std::optional<MeshAssetRecord> get_mesh_record(const MeshAssetHandle& handle) override;
    std::optional<TextureAssetRecord> get_texture_record(const TextureAssetHandle& handle) override;

    bool load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination) override;
    bool load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination) override;

  private:
    const AssetRegistry& m_registry;
};

} // namespace Mizu