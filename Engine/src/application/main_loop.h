#pragma once

namespace Mizu
{

// Forward declarations
class Application;
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

    void run_single_threaded(StateManagerCoordinator& coordinator) const;
    void run_multi_threaded(StateManagerCoordinator& coordinator) const;

    static void sim_loop(StateManagerCoordinator& coordinator);
    static void rend_loop(StateManagerCoordinator& coordinator);
};

} // namespace Mizu