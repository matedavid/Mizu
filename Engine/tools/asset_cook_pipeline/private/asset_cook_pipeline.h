#pragma once

#include <unordered_map>
#include <vector>

#include "package/game_package.h"

#include "asset_cooker.h"

namespace Mizu
{

class AssetCookPipeline
{
  public:
    ~AssetCookPipeline();

    bool init(const GamePackage& package);

    int cook();

  private:
    GamePackage m_package{};

    std::vector<ICookRequestSource*> m_cook_request_sources;
    std::unordered_map<AssetCookType, IAssetCooker*> m_asset_type_to_cooker_map;

    void add_cook_request_source(ICookRequestSource* source);

    void add_asset_cooker(IAssetCooker* cooker);
};

} // namespace Mizu
