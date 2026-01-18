#pragma once

#include <memory>

#include "core/window.h"

namespace Mizu
{

class GameContext
{
  public:
    GameContext(std::shared_ptr<Window> window);

    Window& get_window() const { return *m_window; }
    std::shared_ptr<Window> get_window_ptr() const { return m_window; }

  private:
    std::shared_ptr<Window> m_window;
};

extern GameContext* g_game_context;

void create_game_context(std::shared_ptr<Window> window);
void destroy_game_context();

} // namespace Mizu