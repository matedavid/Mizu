#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "mizu_core_module.h"

namespace Mizu
{

MIZU_CORE_API std::optional<std::filesystem::path> locate_package_manifest(
    std::string_view package_path,
    std::string_view executable_path,
    const char* baked_path);

inline const char* baked_package_manifest_path()
{
#ifdef MIZU_PACKAGE_MANIFEST_PATH
    return MIZU_PACKAGE_MANIFEST_PATH;
#else
    return nullptr;
#endif
}

} // namespace Mizu
