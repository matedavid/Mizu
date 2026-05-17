#include "asset/dev_asset_loader.h"

#include "base/debug/logging.h"

namespace Mizu
{

DevAssetLoader::DevAssetLoader(const AssetRegistry& registry) : m_registry(registry) {}

std::optional<MeshAssetRecord> DevAssetLoader::get_mesh_record(const MeshAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;

    return MeshAssetRecord{};
}

std::optional<TextureAssetRecord> DevAssetLoader::get_texture_record(const TextureAssetHandle& handle)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;

    return TextureAssetRecord{};
}

bool DevAssetLoader::load_mesh_payload(const MeshAssetHandle& handle, std::span<uint8_t> destination)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;
    (void)destination;

    return false;
}

bool DevAssetLoader::load_texture_payload(const TextureAssetHandle& handle, std::span<uint8_t> destination)
{
    const DevAssetLocation location = m_registry.resolve<DevAssetLocation>(handle);
    (void)location;
    (void)destination;

    return false;
}

} // namespace Mizu