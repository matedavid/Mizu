#include <filesystem>
#include <optional>
#include <thread>

#include "base/debug/logging.h"
#include "base/reflection/enum_traits.h"
#include "core/cli/cli_parser.h"
#include "core/package/game_package.h"
#include "core/package/package_locator.h"

#include "asset_cook_pipeline.h"

using namespace Mizu;

int main(int argc, const char* argv[])
{
    constexpr auto cli_parser =
        cli::CliParser{}
            .add_option<"package", "--package", "-p", std::string_view>("The package manifest file")
            .add_option<"num_threads", "--num-threads", "-nt", uint32_t>("Number of threads")
            .add_option<"log_level", "--log-level", "-ll", uint32_t>(
                "Log level. 0 = None, 1 = Info, 2 = Warning, 3 = Error")
            .add_flag<"force_cook", "--force", "-f">("Force cook all assets");

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

    const uint32_t num_threads = result.get_or<"num_threads">(std::thread::hardware_concurrency());
    const uint32_t log_level =
        std::clamp(result.get_or<"log_level">(0u), 0u, static_cast<uint32_t>(meta::enum_count_v<LogLevel>));
    const bool force_cook = result.get_or<"force_cook">(false);

    const CookConfig cook_config{
        .log_level = static_cast<LogLevel>(log_level),
        .num_threads = num_threads,
        .force_cook = force_cook,
    };

    AssetCookPipeline pipeline{};

    if (!pipeline.init(*game_package, cook_config))
        return 1;

    return pipeline.cook();
}
