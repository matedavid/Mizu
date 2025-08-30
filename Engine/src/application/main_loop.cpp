#include "main_loop.h"

#include "application/application.h"
#include "application/thread_sync.h"
#include "application/window.h"

#include "base/debug/logging.h"
#include "base/debug/profiling.h"

#include "renderer/scene_renderer.h"

#include "state_manager/state_manager_coordinator.h"

namespace Mizu
{

MainLoop::MainLoop() : m_application(nullptr), m_run_multi_threaded(true) {}

MainLoop::~MainLoop()
{
    delete m_application;
    delete g_job_system;
}

bool MainLoop::init()
{
    const uint32_t num_threads = std::thread::hardware_concurrency();
    if (num_threads == 1)
    {
        // If has only one thread, can't run multi threaded
        m_run_multi_threaded = false;
    }

#ifdef MIZU_FORCE_SINGLE_THREADED
    m_run_multi_threaded = false;
#endif

    if (m_run_multi_threaded)
    {
        constexpr size_t JOB_SYTEM_CAPACITY = 256;
        g_job_system = new JobSystem(num_threads - 1, JOB_SYTEM_CAPACITY);
        g_job_system->init();
    }

    // Init Application & Renderer
    m_application = create_application();

    return true;
}

void MainLoop::run() const
{
    StateManagerCoordinator coordinator;
    TickInfo tick_info;
    SceneRenderer renderer;

    if (m_run_multi_threaded)
    {
        run_multi_threaded(coordinator, tick_info, renderer);
    }
    else
    {
        run_single_threaded(coordinator);
    }
}

void MainLoop::run_single_threaded(StateManagerCoordinator& coordinator) const
{
    sim_set_thread_id(std::this_thread::get_id());
    rend_set_thread_id(std::this_thread::get_id());
    sim_set_is_running(true);

    Application& application = *Application::instance();
    Window& window = *application.get_window();

    application.on_init();

    SceneRenderer renderer;

    double last_time = window.get_current_time();
    while (!window.should_close())
    {
        const double current_time = window.get_current_time();
        const double ts = current_time - last_time;
        last_time = current_time;

        {
            coordinator.sim_begin_tick();

            application.on_update(ts);

            coordinator.sim_end_tick();
        }

        {
            coordinator.rend_begin_frame();

            renderer.render();

            coordinator.rend_end_frame();
        }

        window.poll_events();
    }

    sim_set_is_running(false);
}

void MainLoop::run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer)
    const
{
    const Job sim_init_job = Job::create([]() {
                                 sim_set_thread_id(std::this_thread::get_id());
                                 sim_set_is_running(true);
                                 Application::instance()->on_init();
                             }).set_affinity(ThreadAffinity_Simulation);
    const Job rend_init_job =
        Job::create([]() { rend_set_thread_id(std::this_thread::get_id()); }).set_affinity(ThreadAffinity_Render);

    const JobSystemHandle sim_init_handle = g_job_system->schedule(sim_init_job);
    const JobSystemHandle rend_init_handle = g_job_system->schedule(rend_init_job);

    sim_init_handle.wait();
    rend_init_handle.wait();

    spawn_main_jobs(coordinator, tick_info, renderer);

    g_job_system->run_thread_as_worker(ThreadAffinity_Main);

    g_job_system->wait_workers_are_dead();
}

void MainLoop::spawn_main_jobs(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer) const
{
    const Job poll_events_job = Job::create([]() {
                                    MIZU_PROFILE_SCOPED_NAME("MainLoop::PollEvents");

                                    Window& window = *Application::instance()->get_window();
                                    window.poll_events();

                                    if (window.should_close())
                                        g_job_system->kill();
                                }).set_affinity(ThreadAffinity_Main);
    const JobSystemHandle poll_events_handle = g_job_system->schedule(poll_events_job);

    const Job sim_job = Job::create(&MainLoop::sim_job, std::ref(coordinator), std::ref(tick_info))
                            .set_affinity(ThreadAffinity_Simulation)
                            .depends_on(poll_events_handle);
    const JobSystemHandle sim_handle = g_job_system->schedule(sim_job);

    const Job rend_job = Job::create(&MainLoop::rend_job, std::ref(coordinator), std::ref(renderer))
                             .set_affinity(ThreadAffinity_Render)
                             .depends_on(sim_handle);
    g_job_system->schedule(rend_job);

    const Job spawn_jobs = Job::create([this, &coordinator, &tick_info, &renderer] {
                               spawn_main_jobs(coordinator, tick_info, renderer);
                           }).depends_on(sim_handle);
    g_job_system->schedule(spawn_jobs);
}

void MainLoop::sim_loop(StateManagerCoordinator& coordinator)
{
    sim_set_thread_id(std::this_thread::get_id());
    sim_set_is_running(true);

    MIZU_PROFILE_SET_THREAD_NAME("Simulation thread");

    Application& application = *Application::instance();
    Window& window = *application.get_window();

    application.on_init();

    double last_time = window.get_current_time();
    while (!window.should_close())
    {
        MIZU_PROFILE_SCOPED;

        const double current_time = window.get_current_time();
        const double ts = current_time - last_time;
        last_time = current_time;

        window.poll_events();

        coordinator.sim_begin_tick();

        application.on_update(ts);

        coordinator.sim_end_tick();
    }

    sim_set_is_running(false);
}

void MainLoop::rend_loop(StateManagerCoordinator& coordinator)
{
    rend_set_thread_id(std::this_thread::get_id());

    MIZU_PROFILE_SET_THREAD_NAME("Render thread");

    const Window& window = *Application::instance()->get_window();

    SceneRenderer renderer;

    while (!window.should_close())
    {
        MIZU_PROFILE_SCOPED;

        coordinator.rend_begin_frame();

        renderer.render();

        coordinator.rend_end_frame();

        window.swap_buffers();

        MIZU_PROFILE_FRAME_MARK;
    }

    Renderer::wait_idle();
}

void MainLoop::sim_job(StateManagerCoordinator& coordinator, TickInfo& tick_info)
{
    MIZU_PROFILE_SCOPED;

    Application& application = *Application::instance();
    Window& window = *application.get_window();

    const double current_time = window.get_current_time();
    const double ts = current_time - tick_info.last_time;
    tick_info.last_time = current_time;

    coordinator.sim_begin_tick();
    {
        application.on_update(ts);
    }
    coordinator.sim_end_tick();
}

void MainLoop::rend_job(StateManagerCoordinator& coordinator, SceneRenderer& renderer)
{
    MIZU_PROFILE_SCOPED;

    Window& window = *Application::instance()->get_window();

    coordinator.rend_begin_frame();
    {
        renderer.render();
    }
    coordinator.rend_end_frame();

    window.swap_buffers();

    MIZU_PROFILE_FRAME_MARK;
}

} // namespace Mizu