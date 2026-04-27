#pragma once

#include "state_manager/state_manager_coordinator.h"

#include "core/job_system/job_system.h"
#include "mizu_core_module.h"

namespace Mizu
{

extern MIZU_CORE_API JobSystem* g_job_system;
extern MIZU_CORE_API StateManagerCoordinator* g_state_manager_coordinator;

} // namespace Mizu
