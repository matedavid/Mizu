#pragma once

#include <chrono>
#include <functional>

namespace Mizu
{

class GameSimulation;

class SimulationLoop
{
  public:
    SimulationLoop(GameSimulation& game_simulation, std::function<void()> shutdown_job);
    ~SimulationLoop() = default;

    void create_update_job();
    void update_job();
    void poll_events_job();

  private:
    GameSimulation& m_game_simulation;
    std::function<void()> m_shutdown_job;
    std::chrono::high_resolution_clock::time_point m_start_time;
    std::chrono::high_resolution_clock::time_point m_last_time;

    void get_elapsed_time(uint64_t& elapsed_us, double& elapsed_secs);

    void recursive_job();
    void shutdown_job();
};

} // namespace Mizu