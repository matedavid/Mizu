#include <optional>

#include "base/debug/logging.h"

#include "package/game_package.h"
#include "package/package_locator.h"

#include "asset_cook_pipeline.h"

using namespace Mizu;

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

    AssetCookPipeline pipeline{};

    if (!pipeline.init(*game_package))
        return 1;

    return pipeline.cook();
}
