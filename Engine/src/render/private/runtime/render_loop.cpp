#include "render/runtime/render_loop.h"

#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"

#include "render/runtime/game_renderer.h"
#include "render/state_manager/light_state_manager.h"
#include "render/state_manager/static_mesh_state_manager.h"
#include "render/state_manager/transform_state_manager.h"

namespace Mizu
{

RenderLoop::RenderLoop(GameRenderer& game_renderer, std::function<void()> shutdown_job)
    : m_game_renderer(game_renderer)
    , m_shutdown_job(std::move(shutdown_job))
{
    m_start_time = std::chrono::high_resolution_clock::now();

    g_transform_state_manager2 = new TransformStateManager2{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_transform_state_manager2));

    g_static_mesh_state_manager2 = new StaticMeshStateManager2{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_static_mesh_state_manager2).depends_on(g_transform_state_manager2));

    g_light_state_manager2 = new LightStateManager2{};
    g_state_manager_coordinator->register_state_manager(
        StateManagerRegistrationBuilder::begin(g_light_state_manager2).depends_on(g_transform_state_manager2));
}

RenderLoop::~RenderLoop()
{
    delete g_light_state_manager2;
    delete g_static_mesh_state_manager2;
    delete g_transform_state_manager2;
}

void RenderLoop::create_update_job()
{
    const Job rend_job = Job::create(&RenderLoop::update_job, this).set_affinity(ThreadAffinity_Render);
    const JobSystemHandle rend_job_handle = g_job_system->schedule(rend_job);

    const Job recursive_job = Job::create(&RenderLoop::recursive_job, this).depends_on(rend_job_handle);
    g_job_system->schedule(recursive_job);
}

void RenderLoop::update_job()
{
    MIZU_PROFILE_SCOPED;

    const uint64_t elapsed_us = get_elapsed_time_us();

    FrameUpdateState frame_state{};
    frame_state.render_time_us = elapsed_us;
    g_state_manager_coordinator->rend_apply_updates(frame_state);

    // TODO: The rest of jobs

    m_game_renderer.render();

    MIZU_PROFILE_FRAME_MARK;
}

uint64_t RenderLoop::get_elapsed_time_us()
{
    const auto time_now = std::chrono::high_resolution_clock::now();

    const auto elapsed_us_signed =
        std::chrono::duration_cast<std::chrono::microseconds>(time_now - m_start_time).count();
    MIZU_ASSERT(elapsed_us_signed >= 0, "Elapsed time must be greater or equal than 0");

    const uint64_t elapsed_us = static_cast<uint64_t>(elapsed_us_signed);
    return elapsed_us;
}

void RenderLoop::recursive_job()
{
    const Window& window = g_game_context->get_window();

    if (!window.should_close())
        create_update_job();
    else
        shutdown_job();
}

void RenderLoop::shutdown_job()
{
    m_shutdown_job();
}

} // namespace Mizu
