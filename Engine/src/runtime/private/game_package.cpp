#include "game_package.h"

#include <fstream>
#include <string>
#include <string_view>

#include "base/debug/logging.h"

namespace Mizu
{

static constexpr std::string_view VERSION_KEY = "version";
static constexpr std::string_view NAME_KEY = "name";
static constexpr std::string_view DISPLAY_NAME_KING = "display_name";
static constexpr std::string_view GAME_ROOT_KEY = "game_root";
static constexpr std::string_view ASSET_MOUNT_KEY = "asset_mount";

static void parse_asset_mount(std::string_view value, GamePackage& package)
{
    const size_t pos = value.find('|');

    if (pos == std::string_view::npos)
    {
        MIZU_LOG_ERROR("Invalid asset mount format: {}", value);
        return;
    }

    const std::string_view name = value.substr(0, pos);
    const std::string_view path = value.substr(pos + 1);

    if (name.empty() || path.empty())
    {
        MIZU_LOG_ERROR("Invalid asset mount format: {}", value);
        return;
    }

    package.asset_mounts.add_asset_mount(
        AssetMount{
            .path = std::filesystem::path{path},
            .name = std::string{name},
        });
}

static void set_value(std::string_view key, std::string_view value, GamePackage& package)
{
    if (key == VERSION_KEY)
    {
        // Ignore version for now
    }
    else if (key == NAME_KEY)
    {
        // Ignore name for now
    }
    else if (key == DISPLAY_NAME_KING)
    {
        package.display_name = std::string{value};
    }
    else if (key == GAME_ROOT_KEY)
    {
        package.root_path = std::filesystem::path{value};
    }
    else if (key == ASSET_MOUNT_KEY)
    {
        parse_asset_mount(value, package);
    }
    else
    {
        MIZU_LOG_ERROR("Unknown GamePackage key: {}", key);
    }
}

static void parse_line(std::string_view line, GamePackage& package)
{
    if (line.empty())
        return;

    const size_t pos = line.find('=');

    if (pos == std::string_view::npos)
        return;

    const std::string_view key = line.substr(0, pos);
    const std::string_view value = line.substr(pos + 1);

    set_value(key, value, package);
}

std::optional<GamePackage> GamePackage::parse(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path))
    {
        MIZU_LOG_ERROR("GamePackage path does not exist: {}", path.string());
        return std::nullopt;
    }

    std::ifstream file{path};
    if (!file.is_open())
    {
        MIZU_LOG_ERROR("Failed to open GamePackage file: {}", path.string());
        return std::nullopt;
    }

    GamePackage package{};

    std::string line{};
    while (std::getline(file, line))
    {
        parse_line(line, package);
    }

    return package;
}

} // namespace Mizu
