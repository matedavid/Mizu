#include "core/package/package_locator.h"

#include <format>
#include <string_view>

#include "base/debug/logging.h"

namespace Mizu
{

std::optional<std::filesystem::path> locate_package_manifest(
    std::string_view package_path,
    std::string_view executable_path,
    const char* baked_path)
{
    if (!package_path.empty())
    {
        if (std::filesystem::exists(package_path))
            return package_path;

        MIZU_LOG_ERROR("Manifest package does not exist: {}", package_path);
        return std::nullopt;
    }

    if (baked_path != nullptr)
    {
        const std::filesystem::path path{baked_path};

        if (std::filesystem::exists(path))
            return path;
    }

    if (!executable_path.empty())
    {
        const std::filesystem::path executable_p{executable_path};

#if MIZU_PLATFORM_WINDOWS
        // In windows we have to remove the .exe
        const std::filesystem::path executable_name = executable_p.stem();
#elif MIZU_PLATFORM_UNIX
        // On linux the executable does not have an extension
        const std::filesystem::path executable_name = executable_p.filename();
#endif
        const std::filesystem::path path =
            executable_p.parent_path() / std::format("{}.manifest.package", executable_name.string());

        if (std::filesystem::exists(path))
            return path;
    }

    return std::nullopt;
}

} // namespace Mizu
