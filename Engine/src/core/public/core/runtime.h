#pragma once

#include "base/threads/job_system.h"
#include "state_manager/state_manager_coordinator.h"

#include "mizu_core_module.h"

namespace Mizu
{

constexpr ThreadAffinity ThreadAffinity_Main = 0;
constexpr ThreadAffinity ThreadAffinity_Simulation = 1;
constexpr ThreadAffinity ThreadAffinity_Render = 2;

extern MIZU_CORE_API JobSystem* g_job_system;

extern MIZU_CORE_API StateManagerCoordinator2* g_state_manager_coordinator2;

} // namespace Mizu
