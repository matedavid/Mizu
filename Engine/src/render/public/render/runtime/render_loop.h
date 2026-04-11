#pragma once

#include <chrono>
#include <functional>

#include "mizu_render_module.h"
#include "render/runtime/render_frame_timing.h"

namespace Mizu
{

class GameRenderer;

class MIZU_RENDER_API RenderLoop
{
  public:
    RenderLoop(GameRenderer& game_renderer, std::function<void()> shutdown_job);
    ~RenderLoop();

    void create_update_job();

  private:
    GameRenderer& m_game_renderer;
    std::function<void()> m_shutdown_job;

    RenderFrameTiming m_frame_timing;
    std::chrono::high_resolution_clock::time_point m_start_time;
    std::chrono::high_resolution_clock::time_point m_last_time;

    void prepare_frame();
    void recursive_job();
    void shutdown_job();

    RenderFrameTiming get_frame_timing();
};

} // namespace Mizu
