#include "core/game_context.h"

#include "asset/asset_loader.h"
#include "asset/asset_registry.h"
#include "base/debug/assert.h"

#include "core/window.h"

namespace Mizu
{

GameContext* g_game_context = nullptr;

void create_game_context(
    std::shared_ptr<Window> window,
    std::shared_ptr<AssetRegistry> asset_registry,
    std::shared_ptr<IAssetLoader> asset_loader)
{
    MIZU_VERIFY(g_game_context == nullptr, "Can only have one instance of GameContext");
    g_game_context = new GameContext(std::move(window), std::move(asset_registry), std::move(asset_loader));
}

void destroy_game_context()
{
    delete g_game_context;
}

} // namespace Mizu