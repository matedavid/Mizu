#include "runtime/main_loop.h"

#include <optional>

#include "game_package.h"

using namespace Mizu;

static std::optional<std::filesystem::path> get_manifest_path(int argc, const char* argv[])
{
    if (argc <= 0)
        return std::nullopt;

    const std::filesystem::path executable_path{argv[0]};

    const std::filesystem::path executable_name = executable_path.stem();
    const std::filesystem::path manifest_path =
        executable_path.parent_path() / std::format("{}.manifest.package", executable_name.string());

    if (std::filesystem::exists(manifest_path))
        return manifest_path;

    return std::nullopt;
}

int main(int argc, const char* argv[])
{
    const std::optional<std::filesystem::path> manifest_path = get_manifest_path(argc, argv);
    if (!manifest_path.has_value())
    {
        MIZU_LOG_ERROR(
            "Failed to find manifest package. Expected at: {}",
            std::filesystem::absolute(manifest_path.value()).string());
        return 1;
    }

    const std::optional<GamePackage> game_package = GamePackage::parse(*manifest_path);
    if (!game_package.has_value())
    {
        MIZU_LOG_ERROR("Failed to parse manifest package at: {}", manifest_path->string());
        return 1;
    }

    MainLoop main_loop{};

    if (!main_loop.init(*game_package))
        return 1;

    main_loop.run();

    return 0;
}
