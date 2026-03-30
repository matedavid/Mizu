#include "state_manager/state_manager_coordinator.h"

namespace Mizu
{

void StateManagerCoordinator::register_state_manager(IStateManager* state_manager)
{
    m_state_managers.push_back(state_manager);
}

} // namespace Mizu