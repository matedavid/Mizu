#pragma once

namespace Mizu
{

class StateManagerCoordinator
{
  public:
    ~StateManagerCoordinator();

    void sim_begin_tick();
    void sim_end_tick();

    void rend_begin_frame();
    void rend_end_frame();
};

} // namespace Mizu