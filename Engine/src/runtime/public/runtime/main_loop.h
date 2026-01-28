#pragma once

#include <atomic>
#include <memory>

namespace Mizu
{

// Forward declarations
class GameMain;
class GameRenderer;
class GameSimulation;
class StateManagerCoordinator;
class Window;
struct GameDescription;

extern GameMain* create_game_main();

class MainLoop
{
  public:
    MainLoop() = default;
    ~MainLoop();

    bool init();
    void run();

  private:
    GameMain* m_game_main;
    GameSimulation* m_game_simulation;
    std::shared_ptr<Window> m_window;

    inline static std::atomic<uint32_t> m_shutdown_counter;

    void init_simulation();
    void init_renderer(const GameDescription& desc);

    struct TickInfo
    {
        double last_time = 0.0;
    };

    void run_single_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info);
    static void spawn_single_threaded_job(
        StateManagerCoordinator& coordinator,
        TickInfo& tick_info,
        Window& window,
        GameSimulation& simulation,
        GameRenderer& renderer);

    void run_multi_threaded(StateManagerCoordinator& coordinator, TickInfo& tick_info);
    static void spawn_multi_threaded_jobs(
        StateManagerCoordinator& coordinator,
        TickInfo& tick_info,
        Window& window,
        GameSimulation& simulation,
        GameRenderer& renderer);

    static void poll_events_job(Window& window);
    static void sim_job(
        StateManagerCoordinator& coordinator,
        TickInfo& tick_info,
        Window& window,
        GameSimulation& simulation);
    static void rend_job(StateManagerCoordinator& coordinator, Window& window, GameRenderer& renderer);
    static void shutdown_job();
};

} // namespace Mizu