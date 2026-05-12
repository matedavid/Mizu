#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "base/containers/inplace_vector.h"

namespace Mizu
{

struct AssetMount
{
    std::filesystem::path path;
    std::string name;
};

struct GamePackage
{
    std::string display_name;
    std::filesystem::path root_path;

    static constexpr size_t MaxAssetMounts = 5;
    inplace_vector<AssetMount, MaxAssetMounts> asset_mounts;

    static std::optional<GamePackage> parse(const std::filesystem::path& path);
};

} // namespace Mizu