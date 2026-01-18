#include "game_context.h"

#include "base/debug/assert.h"

namespace Mizu
{

GameContext* g_game_context = nullptr;

GameContext::GameContext(std::shared_ptr<Window> window) : m_window(std::move(window)) {}

void create_game_context(std::shared_ptr<Window> window)
{
    MIZU_VERIFY(g_game_context == nullptr, "Can only have one instance of GameContext");
    g_game_context = new GameContext(std::move(window));
}

void destroy_game_context()
{
    delete g_game_context;
}

} // namespace Mizu