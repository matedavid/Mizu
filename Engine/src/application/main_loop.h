#pragma once

namespace Mizu
{

// Forward declarations
class Application;
class SceneRenderer;
class StateManagerCoordinator;

extern Application* create_application();

class MainLoop
{
  public:
    MainLoop();
    ~MainLoop();

    bool init();
    void run() const;

  private:
    Application* m_application;
    bool m_run_multi_threaded;

    struct TickInfo
    {
        double last_time = 0.0;
    };

    void run_single_threaded(StateManagerCoordinator& coordinator) const;
    void run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer) const;

    void spawn_main_jobs(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer) const;

    static void sim_loop(StateManagerCoordinator& coordinator);
    static void rend_loop(StateManagerCoordinator& coordinator);

    static void poll_events_job();
    static void sim_job(StateManagerCoordinator& coordinator, TickInfo& tick_info);
    static void rend_job(StateManagerCoordinator& coordinator, SceneRenderer& renderer);
};

} // namespace Mizu