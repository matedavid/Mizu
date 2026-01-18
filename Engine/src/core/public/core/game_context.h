#pragma once

#include <memory>

#include "core/window.h"
#include "mizu_core_module.h"

namespace Mizu
{

class MIZU_CORE_API GameContext
{
  public:
    GameContext(std::shared_ptr<Window> window);

    Window& get_window() const { return *m_window; }
    std::shared_ptr<Window> get_window_ptr() const { return m_window; }

  private:
    std::shared_ptr<Window> m_window;
};

extern MIZU_CORE_API GameContext* g_game_context;

void MIZU_CORE_API create_game_context(std::shared_ptr<Window> window);
void MIZU_CORE_API destroy_game_context();

} // namespace Mizu