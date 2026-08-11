#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "asset/asset.h"

namespace Mizu
{

struct GamePackage
{
    std::string display_name;
    std::filesystem::path root_path;
    AssetMountTable asset_mounts;

    static std::optional<GamePackage> parse(const std::filesystem::path& path);
};

} // namespace Mizu