#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include "asset/asset.h"
#include "mizu_package_module.h"

namespace Mizu
{

struct GamePackage
{
    std::string name;
    std::string display_name;
    std::filesystem::path root_path;
    std::filesystem::path cook_output_path;

    AssetMountTable asset_mounts;

    MIZU_PACKAGE_API static std::optional<GamePackage> parse(const std::filesystem::path& path);
};

} // namespace Mizu
