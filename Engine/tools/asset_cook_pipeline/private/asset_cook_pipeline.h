#pragma once

#include "package/game_package.h"

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
};

} // namespace Mizu
