#include "render/runtime/render_loop.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"
#include "core/window.h"

#include "render/runtime/game_renderer.h"
#include "state_manager/state_manager_coordinator.h"

namespace Mizu
{

RenderLoop::RenderLoop(GameRenderer& game_renderer, std::function<void()> shutdown_job)
    : m_game_renderer(game_renderer)
    , m_shutdown_job(std::move(shutdown_job))
{
    m_start_time = std::chrono::high_resolution_clock::now();
    m_last_time = m_start_time;
}

void RenderLoop::create_update_jobs()
{
    const JobHandle prepare_frame_job =
        g_job_system->schedule(&RenderLoop::prepare_frame, this).name("RenderPrepareFrame").submit();
    const JobHandle rend_job = m_game_renderer.create_update_jobs(prepare_frame_job);
    g_job_system->schedule(&RenderLoop::recursive_job, this).depends_on(rend_job).name("RenderRecursiveJob").submit();
}

void RenderLoop::prepare_frame()
{
    MIZU_PROFILE_SCOPED;

    m_game_renderer.acquire_swapchain_image();

    m_frame_timing = get_frame_timing();
    m_game_renderer.set_frame_timing(m_frame_timing);

    FrameUpdateState frame_state{};
    frame_state.render_time_us = m_frame_timing.render_time_us;
    g_state_manager_coordinator->rend_apply_updates(frame_state);
}

void RenderLoop::recursive_job()
{
    const Window& window = g_game_context->get_window();

    if (!window.should_close())
        create_update_jobs();
    else
        shutdown_job();
}

void RenderLoop::shutdown_job()
{
    m_shutdown_job();
}

RenderFrameTiming RenderLoop::get_frame_timing()
{
    const auto time_now = std::chrono::high_resolution_clock::now();
    const auto frame_delta = time_now - m_last_time;

    const auto elapsed_us_signed =
        std::chrono::duration_cast<std::chrono::microseconds>(time_now - m_start_time).count();
    MIZU_ASSERT(elapsed_us_signed >= 0, "Elapsed time must be greater or equal than 0");

    m_last_time = time_now;

    RenderFrameTiming frame_timing{};
    frame_timing.render_time_us = static_cast<uint64_t>(elapsed_us_signed);
    frame_timing.frame_delta_seconds = std::chrono::duration<double>(frame_delta).count();

    return frame_timing;
}

} // namespace Mizu
