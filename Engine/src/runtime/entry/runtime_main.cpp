#include "runtime/main_loop.h"

#include <optional>
#include <span>
#include <string_view>

#include "base/debug/logging.h"
#include "core/settings_manager/settings_manager.h"
#include "render/runtime/renderer_settings.h"

#include "package/game_package.h"
#include "package/package_locator.h"

using namespace Mizu;

static bool split_setting_member(
    std::string_view arg,
    std::string_view& out_setting_name,
    std::string_view& out_member_name)
{
    const auto p = arg.find('.');
    if (p == std::string_view::npos || p == 0 || p == arg.size() - 1)
        return false;

    out_setting_name = arg.substr(0, p);
    out_member_name = arg.substr(p + 1);

    return true;
}

static void parse_setting_args(std::span<const std::string_view> args)
{
    for (size_t idx = 0; idx < args.size(); ++idx)
    {
        std::string_view arg = args[idx];

        std::string_view value;
        bool has_value = false;

        // <Setting>.<member>=<value>
        if (const auto eq = arg.find('='); eq != std::string_view::npos)
        {
            value = arg.substr(eq + 1);
            arg = arg.substr(0, eq);
            has_value = true;
        }

        std::string_view setting_name;
        std::string_view member_name;
        if (!split_setting_member(arg, setting_name, member_name))
        {
            // Not a setting argument, so it must not consume the next argument as its value
            MIZU_LOG_ERROR("Ignoring argument '{}', expected the format '<Setting>.<member>'", arg);
            continue;
        }

        // <Setting>.<member> <value>
        if (!has_value)
        {
            if (idx + 1 >= args.size())
            {
                MIZU_LOG_ERROR("Ignoring argument '{}', it does not have a value", arg);
                break;
            }

            value = args[idx + 1];
            idx += 1;
        }

        SettingsManager::get().set_member_value_from_string(setting_name, member_name, value);
    }
}

int main(int argc, const char* argv[])
{
    const EngineCommandLine command_line = parse_engine_command_line(argc, argv);

    const std::optional<std::filesystem::path> manifest_path =
        locate_package_manifest(command_line, baked_package_manifest_path());
    if (!manifest_path.has_value())
    {
        MIZU_LOG_ERROR("Failed to find manifest package");
        return 1;
    }

    const std::optional<GamePackage> game_package = GamePackage::parse(*manifest_path);
    if (!game_package.has_value())
    {
        MIZU_LOG_ERROR("Failed to parse manifest package at: {}", manifest_path->string());
        return 1;
    }

    parse_setting_args(command_line.rest);

    MainLoop main_loop{};

    if (!main_loop.init(*game_package))
        return 1;

    main_loop.run();

    return 0;
}
