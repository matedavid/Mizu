#include "package/package_locator.h"

#include <format>
#include <string_view>

#include "base/debug/logging.h"

namespace Mizu
{

static constexpr std::string_view PACKAGE_ARG = "--package";

EngineCommandLine parse_engine_command_line(int argc, const char* argv[])
{
    EngineCommandLine command_line{};

    if (argc > 0)
        command_line.executable_path = std::filesystem::path{argv[0]};

    for (int idx = 1; idx < argc; ++idx)
    {
        const std::string_view arg = argv[idx];

        // --package=<path>
        if (arg.starts_with(PACKAGE_ARG) && arg.size() > PACKAGE_ARG.size() && arg[PACKAGE_ARG.size()] == '=')
        {
            command_line.package_path = std::filesystem::path{arg.substr(PACKAGE_ARG.size() + 1)};
            continue;
        }

        // --package <path>
        if (arg == PACKAGE_ARG)
        {
            if (idx + 1 >= argc)
            {
                MIZU_LOG_ERROR("Ignoring argument '{}', it does not have a value", arg);
                break;
            }

            command_line.package_path = std::filesystem::path{argv[idx + 1]};
            idx += 1;
            continue;
        }

        command_line.rest.push_back(arg);
    }

    return command_line;
}

std::optional<std::filesystem::path> locate_package_manifest(
    const EngineCommandLine& command_line,
    const char* baked_path)
{
    if (command_line.package_path.has_value())
    {
        if (std::filesystem::exists(*command_line.package_path))
            return *command_line.package_path;

        MIZU_LOG_ERROR("Manifest package does not exist: {}", command_line.package_path->string());
        return std::nullopt;
    }

    if (baked_path != nullptr)
    {
        const std::filesystem::path path{baked_path};

        if (std::filesystem::exists(path))
            return path;
    }

    if (!command_line.executable_path.empty())
    {
#if MIZU_PLATFORM_WINDOWS
        // In windows we have to remove the .exe
        const std::filesystem::path executable_name = command_line.executable_path.stem();
#elif MIZU_PLATFORM_UNIX
        // On linux the executable does not have an extension
        const std::filesystem::path executable_name = command_line.executable_path.filename();
#endif
        const std::filesystem::path path =
            command_line.executable_path.parent_path() / std::format("{}.manifest.package", executable_name.string());

        if (std::filesystem::exists(path))
            return path;
    }

    return std::nullopt;
}

} // namespace Mizu
