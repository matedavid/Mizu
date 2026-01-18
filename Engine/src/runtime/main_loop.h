#pragma once

#include <atomic>
#include <memory>

namespace Mizu
{

// Forward declarations
class Application;
class SceneRenderer;
class StateManagerCoordinator;
class Window;

extern Application* create_application();

class MainLoop
{
  public:
    MainLoop();
    ~MainLoop();

    bool init();
    void run();

  private:
    Application* m_application;
    std::shared_ptr<Window> m_window;

    inline static std::atomic<uint32_t> m_shutdown_counter;

    struct TickInfo
    {
        double last_time = 0.0;
    };

    void run_single_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer);
    static void spawn_single_threaded_job(
        StateManagerCoordinator& coordinator,
        TickInfo& tick_info,
        Window& window,
        SceneRenderer& renderer);

    void run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer);
    static void spawn_multi_threaded_jobs(
        StateManagerCoordinator& coordinator,
        TickInfo& tick_info,
        Window& window,
        SceneRenderer& renderer);

    static void poll_events_job(Window& window);
    static void sim_job(StateManagerCoordinator& coordinator, TickInfo& tick_info, Window& window);
    static void rend_job(StateManagerCoordinator& coordinator, SceneRenderer& renderer, Window& window);
    static void shutdown_job();
};

} // namespace Mizu