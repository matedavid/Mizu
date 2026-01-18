#pragma once

#include <thread>

#include "base/threads/job_system.h"

#include "mizu_core_module.h"

namespace Mizu
{

extern MIZU_CORE_API JobSystem* g_job_system;

constexpr ThreadAffinity ThreadAffinity_Main = 0;
constexpr ThreadAffinity ThreadAffinity_Simulation = 1;
constexpr ThreadAffinity ThreadAffinity_Render = 2;

} // namespace Mizu