#pragma once

#include "asset_cooker.h"

namespace Mizu
{

class MeshImporter : public IAssetImporter
{
  public:
    ~MeshImporter() override = default;

    std::span<std::string_view> extensions() const override;

    uint32_t import(const ImportRequest& input, std::vector<CookRequest>& outputs) const override;
};

class MeshCooker : public IAssetCooker
{
  public:
    AssetCookType asset_type() const override { return AssetCookType::Mesh; }

    bool cook(const AssetCookInfoT& cook_info) const override;
};

} // namespace Mizu