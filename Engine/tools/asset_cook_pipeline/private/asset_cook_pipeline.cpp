#include "asset_cook_pipeline.h"

namespace Mizu
{

AssetCookPipeline::~AssetCookPipeline() {}

bool AssetCookPipeline::init(const GamePackage& package)
{
    m_package = package;

    return true;
}

int AssetCookPipeline::cook()
{
    return 0;
}

} // namespace Mizu
