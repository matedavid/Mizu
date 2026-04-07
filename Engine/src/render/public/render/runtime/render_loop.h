#pragma once

#include <chrono>
#include <functional>

#include "mizu_render_module.h"

namespace Mizu
{

class GameRenderer;

class MIZU_RENDER_API RenderLoop
{
  public:
    RenderLoop(GameRenderer& game_renderer, std::function<void()> shutdown_job);
    ~RenderLoop();

    void create_update_job();
    void update_job();

  private:
    GameRenderer& m_game_renderer;
    std::function<void()> m_shutdown_job;
    std::chrono::high_resolution_clock::time_point m_start_time;

    uint64_t get_elapsed_time_us();

    void recursive_job();
    void shutdown_job();
};

} // namespace Mizu
