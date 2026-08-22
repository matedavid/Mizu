#pragma once

#include <memory>

#include "asset_cooker.h"

// clang-format off
namespace Assimp { class Importer; }
struct aiMesh;
// clang-format on

namespace Mizu
{

struct MeshCookPayload
{
    // Storing here to keep reference to the importer alive while the scene is being used
    std::shared_ptr<Assimp::Importer> importer;
    const aiMesh* mesh;
};

class MeshImporter : public IAssetImporter
{
  public:
    std::span<const std::string_view> extensions() const override;
    uint32_t version() const override;

    bool should_import(const ImportRequest& request, const TimestampDb& timestamp_db) const override;
    void import(const ImportRequest& request, const CookContext& context, std::vector<CookRequest>& outputs) override;

  private:
};

class MeshCooker : public IAssetCooker
{
  public:
    AssetType asset_type() const override { return AssetType::Mesh; }

    bool should_cook(const CookRequest& request, const TimestampDb& timestamp_db) const override;
    void cook(const CookRequest& request, std::vector<SinkRequest>& outputs) override;
};

} // namespace Mizu
