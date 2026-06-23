#include "core/game_context.h"

#include "asset/asset_registry.h"
#include "base/debug/assert.h"

#include "core/window.h"

namespace Mizu
{

GameContext* g_game_context = nullptr;

GameContext::GameContext(std::shared_ptr<Window> window, std::shared_ptr<AssetRegistry> asset_registry)
    : m_window(std::move(window))
    , m_asset_registry(std::move(asset_registry))
{
}

void create_game_context(std::shared_ptr<Window> window, std::shared_ptr<AssetRegistry> asset_registry)
{
    MIZU_VERIFY(g_game_context == nullptr, "Can only have one instance of GameContext");
    g_game_context = new GameContext(std::move(window), std::move(asset_registry));
}

void destroy_game_context()
{
    delete g_game_context;
}

} // namespace Mizu