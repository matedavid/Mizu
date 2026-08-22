#pragma once

#include <memory>

#include "asset_cooker.h"

// clang-format off
namespace Assimp { class Importer; }
struct aiMaterial;
// clang-format on

namespace Mizu
{

struct MaterialCookPayload
{
    // Storing here to keep reference to the importer alive while the scene is being used
    std::shared_ptr<Assimp::Importer> importer;
    const aiMaterial* material;
};

class MaterialCooker : public IAssetCooker
{
  public:
    AssetType asset_type() const override { return AssetType::Material; }

    bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const override;
    void cook(const CookRequest& request, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu
