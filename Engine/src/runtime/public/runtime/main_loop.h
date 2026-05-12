#pragma once

#include <atomic>
#include <memory>
#include <string_view>

namespace Mizu
{

// Forward declarations
class GameMain;
class GameSimulation;
class RenderLoop;
class SimulationLoop;
class Window;
struct GameDescription;
struct GamePackage;

extern GameMain* create_game_main();

class MainLoop
{
  public:
    MainLoop() = default;
    ~MainLoop();

    bool init(const GamePackage& package);
    void run();

  private:
    GameMain* m_game_main;
    GameSimulation* m_game_simulation;
    std::shared_ptr<Window> m_window;

    inline static std::atomic<uint32_t> m_shutdown_counter;

    void init_renderer(const GameDescription& desc, std::string_view name);
    void init_simulation();

    void run_multi_threaded(SimulationLoop& simulation_loop, RenderLoop& render_loop);

    static void shutdown_job();
};

} // namespace Mizu