#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "asset/asset.h"
#include "base/containers/inplace_vector.h"

namespace Mizu
{

struct GamePackage
{
    std::string display_name;
    std::filesystem::path root_path;
    inplace_vector<AssetMount, MaxAssetMounts> asset_mounts;

    static std::optional<GamePackage> parse(const std::filesystem::path& path);
};

} // namespace Mizu