#include "runtime/main_loop.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>

#include "base/debug/logging.h"
#include "core/cli/cli_parser.h"
#include "core/package/game_package.h"
#include "core/package/package_locator.h"
#include "core/settings_manager/settings_manager.h"

using namespace Mizu;

static void parse_setting_args(std::span<const cli::UnclaimedOption> options)
{
    const auto split_setting_member =
        [](std::string_view arg, std::string_view& out_setting_name, std::string_view& out_member_name) {
            const auto p = arg.find('.');
            if (p == std::string_view::npos || p == 0 || p == arg.size() - 1)
                return false;

            out_setting_name = arg.substr(0, p);
            out_member_name = arg.substr(p + 1);

            return true;
        };

    for (const cli::UnclaimedOption& option : options)
    {
        std::string_view setting_name;
        std::string_view member_name;
        if (!split_setting_member(option.name, setting_name, member_name))
        {
            MIZU_LOG_ERROR("Ignoring option '{}', expected the format '<Setting>.<member>'", option.name);
            continue;
        }

        SettingsManager::get().set_member_value_from_string(setting_name, member_name, option.value);
    }
}

int main(int argc, const char* argv[])
{
    constexpr auto cli_parser =
        cli::CliParser{}.add_option<"package", "--package", "-p", std::string_view>("The package manifest file");

    const auto result = cli_parser.parse(argc, argv);
    if (result.help_requested() || result.parse_failed())
    {
        std::cout << result.help_text();
        return result.parse_failed() ? 1 : 0;
    }

    const std::string_view package_path = result.get_or<"package">("");

    const std::optional<std::filesystem::path> manifest_path =
        locate_package_manifest(package_path, argv[0], baked_package_manifest_path());
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

    parse_setting_args(result.get_unclaimed_options());

    MainLoop main_loop{};

    if (!main_loop.init(*game_package))
        return 1;

    main_loop.run();

    return 0;
}
