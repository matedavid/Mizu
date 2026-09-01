#pragma once

#include <filesystem>
#include <format>
#include <string>

#include "asset/asset.h"

namespace Mizu
{

inline std::filesystem::path normalize_path(const std::filesystem::path& path)
{
    // lexically_normal().generic_string() normalizes the path only one, right slash, for paths
    return path.lexically_normal().generic_string();
}

inline std::string create_virtual_path(const std::filesystem::path& path, const AssetMount& mount)
{
    return std::format("{}:{}", mount.name, normalize_path(path).string());
}

} // namespace Mizu