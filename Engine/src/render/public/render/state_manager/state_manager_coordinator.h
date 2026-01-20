#pragma once

#include "mizu_render_module.h"

namespace Mizu
{

class MIZU_RENDER_API StateManagerCoordinator
{
  public:
    StateManagerCoordinator();
    ~StateManagerCoordinator();

    void sim_begin_tick();
    void sim_end_tick();

    void rend_begin_frame();
    void rend_end_frame();
};

} // namespace Mizu