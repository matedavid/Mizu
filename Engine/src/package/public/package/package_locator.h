#pragma once

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include "mizu_package_module.h"

namespace Mizu
{

struct EngineCommandLine
{
    std::filesystem::path executable_path;
    std::optional<std::filesystem::path> package_path;
    std::vector<std::string_view> rest;
};

MIZU_PACKAGE_API EngineCommandLine parse_engine_command_line(int argc, const char* argv[]);

MIZU_PACKAGE_API std::optional<std::filesystem::path> locate_package_manifest(
    const EngineCommandLine& command_line,
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
