#include "runtime/main_loop.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"
#include "core/game_context.h"
#include "core/thread_sync.h"
#include "core/window.h"
#include "render/runtime/renderer.h"
#include "render/state_manager/state_manager_coordinator.h"
#include "state_manager/base_state_manager.h"

#include "runtime/game_main.h"

namespace Mizu
{

MainLoop::~MainLoop()
{
    delete g_game_renderer;

    destroy_game_context();
    delete g_job_system;
}

bool MainLoop::init()
{
    // Init Logging
    MIZU_LOG_SETUP;

    // Init JobSystem
    const uint32_t num_threads = std::thread::hardware_concurrency();
    MIZU_VERIFY(num_threads >= 4, "At least 4 threads are required to run the engine");

    constexpr uint32_t JOB_SYSTEM_THREADS = 4; // num_threads - 1
    constexpr size_t JOB_SYSTEM_CAPACITY = 256;

    g_job_system = new JobSystem(JOB_SYSTEM_THREADS, JOB_SYSTEM_CAPACITY);
    g_job_system->init();

    // Create Game Main
    m_game_main = create_game_main();
    MIZU_ASSERT(m_game_main != nullptr, "GameMain is nullptr");

    const GameDescription& game_desc = m_game_main->get_game_description();

    m_window = std::make_shared<Window>(game_desc.name, game_desc.width, game_desc.height, game_desc.graphics_api);
    create_game_context(m_window);

    // Init Renderer
    init_renderer(game_desc);

    // Init Simulation
    init_simulation();

    return true;
}

void MainLoop::init_renderer(const GameDescription& desc)
{
    GameRendererDescription renderer_desc{};
    renderer_desc.graphics_api = desc.graphics_api;
    renderer_desc.window = m_window;
    renderer_desc.application_name = desc.name;
    renderer_desc.application_version = desc.version;

    g_game_renderer = new GameRenderer{renderer_desc};
    m_game_main->setup_game_renderer(*g_game_renderer);
}

void MainLoop::init_simulation()
{
    m_game_simulation = m_game_main->create_game_simulation();
    MIZU_ASSERT(m_game_simulation != nullptr, "GameSimulation is nullptr");

    m_window->add_event_callback_func([&](Event& event) {
        switch (event.get_type())
        {
        case EventType::WindowResized: {
            auto& window_resized = static_cast<WindowResizedEvent&>(event);
            m_game_simulation->on_window_resized(window_resized);
            break;
        }
        case EventType::MouseMoved: {
            auto& mouse_moved = static_cast<MouseMovedEvent&>(event);
            m_game_simulation->on_mouse_moved(mouse_moved);
            break;
        }
        case EventType::MouseButtonPressed: {
            auto& mouse_pressed = static_cast<MousePressedEvent&>(event);
            m_game_simulation->on_mouse_pressed(mouse_pressed);
            break;
        }
        case EventType::MouseButtonReleased: {
            auto& mouse_released = static_cast<MouseReleasedEvent&>(event);
            m_game_simulation->on_mouse_released(mouse_released);
            break;
        }
        case EventType::MouseScrolled: {
            auto& mouse_scrolled = static_cast<MouseScrolledEvent&>(event);
            m_game_simulation->on_mouse_scrolled(mouse_scrolled);
            break;
        }
        case EventType::KeyPressed: {
            auto& key_pressed = static_cast<KeyPressedEvent&>(event);
            m_game_simulation->on_key_pressed(key_pressed);
            break;
        }
        case EventType::KeyReleased: {
            auto& key_released = static_cast<KeyReleasedEvent&>(event);
            m_game_simulation->on_key_released(key_released);
            break;
        }
        case EventType::KeyRepeated: {
            auto& key_repeat = static_cast<KeyRepeatEvent&>(event);
            m_game_simulation->on_key_repeat(key_repeat);
            break;
        }
        }
    });
}

void MainLoop::run()
{
    StateManagerCoordinator coordinator;
    TickInfo tick_info;

#ifdef MIZU_MAIN_LOOP_SINGLE_THREADED
    run_single_threaded(coordinator, tick_info, renderer);
#else
    run_multi_threaded(coordinator, tick_info);
#endif
}

void MainLoop::run_single_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info)
{
    m_game_simulation->init();

    m_shutdown_counter.store(1);

    spawn_single_threaded_job(coordinator, tick_info, *m_window, *m_game_simulation, *g_game_renderer);

    g_job_system->run_thread_as_worker(ThreadAffinity_Main);

    g_job_system->wait_workers_are_dead();
}

void MainLoop::spawn_single_threaded_job(
    StateManagerCoordinator& coordinator,
    TickInfo& tick_info,
    Window& window,
    GameSimulation& simulation,
    GameRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    const Job poll_events_job =
        Job::create(&MainLoop::poll_events_job, std::ref(window)).set_affinity(ThreadAffinity_Main);
    const JobSystemHandle poll_events_handle = g_job_system->schedule(poll_events_job);

    const Job work_job = Job::create([&]() {
                             sim_job(coordinator, tick_info, window, simulation);
                             rend_job(coordinator, window, renderer);
                         })
                             .set_affinity(ThreadAffinity_Simulation)
                             .depends_on(poll_events_handle);
    const JobSystemHandle handle = g_job_system->schedule(work_job);

    if (!window.should_close())
    {
        const Job spawn_job = Job::create(
                                  &MainLoop::spawn_single_threaded_job,
                                  std::ref(coordinator),
                                  std::ref(tick_info),
                                  std::ref(window),
                                  std::ref(simulation),
                                  std::ref(renderer))
                                  .depends_on(handle);
        g_job_system->schedule(spawn_job);
    }
    else
    {
        const Job shutdown_job =
            Job::create(&MainLoop::shutdown_job).set_affinity(ThreadAffinity_Main).depends_on(handle);
        g_job_system->schedule(shutdown_job);
    }
}

void MainLoop::run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info)
{
    m_game_simulation->init();

    const uint32_t states_in_flight = BaseStateManagerConfig::MaxStatesInFlight;
    m_shutdown_counter.store(states_in_flight);

    for (uint32_t i = 0; i < states_in_flight; ++i)
        spawn_multi_threaded_jobs(coordinator, tick_info, *m_window, *m_game_simulation, *g_game_renderer);

    g_job_system->run_thread_as_worker(ThreadAffinity_Main);

    g_job_system->wait_workers_are_dead();
}

void MainLoop::spawn_multi_threaded_jobs(
    StateManagerCoordinator& coordinator,
    TickInfo& tick_info,
    Window& window,
    GameSimulation& simulation,
    GameRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    const Job poll_events_job =
        Job::create(&MainLoop::poll_events_job, std::ref(window)).set_affinity(ThreadAffinity_Main);
    const JobSystemHandle poll_events_handle = g_job_system->schedule(poll_events_job);

    const Job sim_job =
        Job::create(
            &MainLoop::sim_job, std::ref(coordinator), std::ref(tick_info), std::ref(window), std::ref(simulation))
            .set_affinity(ThreadAffinity_Simulation)
            .depends_on(poll_events_handle);
    const JobSystemHandle sim_handle = g_job_system->schedule(sim_job);

    const Job rend_job = Job::create(&MainLoop::rend_job, std::ref(coordinator), std::ref(window), std::ref(renderer))
                             .set_affinity(ThreadAffinity_Render)
                             .depends_on(sim_handle);
    const JobSystemHandle rend_handle = g_job_system->schedule(rend_job);

    if (!window.should_close())
    {
        const Job spawn_jobs = Job::create(
                                   &MainLoop::spawn_multi_threaded_jobs,
                                   std::ref(coordinator),
                                   std::ref(tick_info),
                                   std::ref(window),
                                   std::ref(simulation),
                                   std::ref(renderer))
                                   .depends_on(rend_handle);
        g_job_system->schedule(spawn_jobs);
    }
    else
    {
        const Job shutdown_job =
            Job::create(&MainLoop::shutdown_job).set_affinity(ThreadAffinity_Main).depends_on(rend_handle);
        g_job_system->schedule(shutdown_job);
    }
}

void MainLoop::poll_events_job(Window& window)
{
    MIZU_PROFILE_SCOPED;

    window.poll_events();
}

void MainLoop::sim_job(
    StateManagerCoordinator& coordinator,
    TickInfo& tick_info,
    Window& window,
    GameSimulation& simulation)
{
    MIZU_PROFILE_SCOPED;

    const double current_time = window.get_current_time();
    const double ts = current_time - tick_info.last_time;
    tick_info.last_time = current_time;

    coordinator.sim_begin_tick();
    {
        simulation.update(ts);
    }
    coordinator.sim_end_tick();
}

void MainLoop::rend_job(StateManagerCoordinator& coordinator, Window& window, GameRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    coordinator.rend_begin_frame();
    {
        renderer.render();
    }
    coordinator.rend_end_frame();

    window.swap_buffers();

    MIZU_PROFILE_FRAME_MARK;
}

void MainLoop::shutdown_job()
{
    MIZU_PROFILE_SCOPED;

    const uint32_t prev_counter = m_shutdown_counter.fetch_sub(1, std::memory_order_relaxed);
    if (prev_counter - 1 == 0)
        g_job_system->kill();
}

} // namespace Mizu