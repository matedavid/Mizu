#pragma once

#include <vector>

#include "mizu_state_manager_module.h"
#include "state_manager.h"

namespace Mizu
{

class MIZU_STATE_MANAGER_API StateManagerCoordinator
{
  public:
    void register_state_manager(IStateManager* state_manager);

  private:
    std::vector<IStateManager*> m_state_managers;
};

} // namespace Mizu