#pragma once

#include <filesystem>
#include <string>

namespace Mizu
{

constexpr size_t MaxAssetMounts = 5;

struct AssetMount
{
    std::filesystem::path path;
    std::string name;
};

} // namespace Mizu