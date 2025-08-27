#pragma once

#include <thread>

#include "base/threads/job_system.h"

namespace Mizu
{

extern JobSystem* g_job_system;

constexpr ThreadAffinity ThreadAffinity_Main = 0;
constexpr ThreadAffinity ThreadAffinity_Simulation = 1;
constexpr ThreadAffinity ThreadAffinity_Render = 2;

void sim_set_thread_id(std::thread::id id);
std::thread::id sim_get_thread_id();
bool is_sim_thread();

void rend_set_thread_id(std::thread::id id);
std::thread::id rend_get_thread_id();
bool is_rend_thread();

void sim_set_is_running(bool is_running);
bool rend_get_is_running();

} // namespace Mizu