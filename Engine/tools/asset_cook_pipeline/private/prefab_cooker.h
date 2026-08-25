#pragma once

#include <vector>

#include "asset/asset_handle.h"

#include "asset_cooker.h"

namespace Mizu
{

struct PrefabMeshInfo
{
    MeshAssetHandle mesh_handle;
    MaterialAssetHandle material_handle;
};

struct PrefabCookPayload
{
    std::vector<PrefabMeshInfo> mesh_info;
};

class PrefabCooker : public IAssetCooker
{
  public:
    AssetType asset_type() const override { return AssetType::Prefab; }

    bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const override;
    void cook(const CookRequest& request, const CookContext& context, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu