#include "entry_point.h"

#include "application/application.h"
#include "application/thread_sync.h"
#include "application/window.h"

#include "base/debug/assert.h"

#include "renderer/scene_renderer.h"

#include "render_core/rhi/renderer.h"

#include "state_manager/transform_state_manager.h"

using namespace Mizu;

static void rend_thread_func()
{
    rend_set_thread_id(std::this_thread::get_id());

    const Window& window = *Application::instance()->get_window();

    SceneRenderer renderer;

    while (!window.should_close())
    {
        g_transform_state_manager->rend_begin_frame();

        renderer.render();

        g_transform_state_manager->rend_end_frame();

        window.swap_buffers();
    }

    Renderer::wait_idle();
}

static void sim_thread_func()
{
    sim_set_thread_id(std::this_thread::get_id());
    sim_set_is_running(true);

    Application* application = Application::instance();
    Window& window = *application->get_window();

    double last_time = window.get_current_time();
    while (!window.should_close())
    {
        const double current_time = window.get_current_time();
        const double ts = current_time - last_time;
        last_time = current_time;

        g_transform_state_manager->sim_begin_tick();

        application->on_update(ts);

        g_transform_state_manager->sim_end_tick();

        window.poll_events();
    }

    sim_set_is_running(false);
}

int main()
{
    Application* application = create_application();

    std::thread rend_thread(rend_thread_func);
    sim_thread_func();

    rend_thread.join();

    delete application;

    return 0;
}
