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

    // Init Application & Renderer
    m_application = create_application();

    return true;
}

void MainLoop::run() const
{
    StateManagerCoordinator coordinator;

    if (m_run_multi_threaded)
    {
        run_multi_threaded(coordinator);
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

void MainLoop::run_multi_threaded(StateManagerCoordinator& coordinator) const
{
    std::thread rend_thread(rend_loop, std::ref(coordinator));
    sim_loop(coordinator);

    rend_thread.join();
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

        coordinator.sim_begin_tick();

        application.on_update(ts);

        coordinator.sim_end_tick();

        window.poll_events();
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

} // namespace Mizu