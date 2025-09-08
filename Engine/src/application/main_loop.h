#pragma once

#include <atomic>

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

    inline static std::atomic<uint32_t> m_shutdown_counter;

    struct TickInfo
    {
        double last_time = 0.0;
    };

    static void run_single_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer);
    static void run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer);

    static void spawn_main_jobs(StateManagerCoordinator& coordinator, TickInfo& tick_info, SceneRenderer& renderer);

    static void poll_events_job();
    static void sim_job(StateManagerCoordinator& coordinator, TickInfo& tick_info);
    static void rend_job(StateManagerCoordinator& coordinator, SceneRenderer& renderer);
    static void shutdown_job();
};

} // namespace Mizu