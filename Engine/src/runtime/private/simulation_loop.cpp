#include "runtime/simulation_loop.h"

#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/runtime.h"

#include "runtime/game_simulation.h"

namespace Mizu
{

SimulationLoop::SimulationLoop(GameSimulation& game_simulation, std::function<void()> shutdown_job)
    : m_game_simulation(game_simulation)
    , m_shutdown_job(std::move(shutdown_job))
{
    m_start_time = std::chrono::high_resolution_clock::now();
    m_last_time = m_start_time;
}

void SimulationLoop::init()
{
    TickUpdateState tick_state{};
    tick_state.sim_time_us = 0;
    g_state_manager_coordinator->sim_begin_tick(tick_state);

    m_game_simulation.init();

    g_state_manager_coordinator->sim_end_tick();
}

void SimulationLoop::create_update_job()
{
    const JobHandle poll_events_job = g_job_system->schedule(&SimulationLoop::poll_events_job, this)
                                          .affinity(JobAffinity::Main)
                                          .name("PollEvents")
                                          .submit();

    const JobHandle sim_job = g_job_system->schedule(&SimulationLoop::update_job, this)
                                  .depends_on(poll_events_job)
                                  .name("SimulationUpdateJob")
                                  .submit();

    g_job_system->schedule(&SimulationLoop::recursive_job, this)
        .depends_on(sim_job)
        .name("SimulationRecursiveJob")
        .submit();
}

void SimulationLoop::update_job()
{
    MIZU_PROFILE_SCOPED;

    uint64_t elapsed_us;
    double elapsed_secs;
    get_elapsed_time(elapsed_us, elapsed_secs);

    TickUpdateState tick_state{};
    tick_state.sim_time_us = elapsed_us;
    g_state_manager_coordinator->sim_begin_tick(tick_state);

    m_game_simulation.update(elapsed_secs);

    g_state_manager_coordinator->sim_end_tick();
}

void SimulationLoop::poll_events_job()
{
    MIZU_PROFILE_SCOPED;

    Window& window = g_game_context->get_window();
    window.poll_events();
}

void SimulationLoop::get_elapsed_time(uint64_t& elapsed_us, double& elapsed_secs)
{
    const auto time_now = std::chrono::high_resolution_clock::now();

    const auto elapsed = time_now - m_last_time;

    const auto elapsed_us_signed =
        std::chrono::duration_cast<std::chrono::microseconds>(time_now - m_start_time).count();
    MIZU_ASSERT(elapsed_us_signed >= 0, "Elapsed time must be greater or equal than 0");

    const auto elapsed_secs_precise = std::chrono::duration<double>(elapsed).count();

    m_last_time = time_now;

    elapsed_us = static_cast<uint64_t>(elapsed_us_signed);
    elapsed_secs = elapsed_secs_precise;
}

void SimulationLoop::recursive_job()
{
    const Window& window = g_game_context->get_window();

    if (!window.should_close())
        create_update_job();
    else
        shutdown_job();
}

void SimulationLoop::shutdown_job()
{
    m_shutdown_job();
}

} // namespace Mizu
