#pragma once

#include <thread>

namespace Mizu
{

void sim_set_thread_id(std::thread::id id);
std::thread::id sim_get_thread_id();
bool is_sim_thread();

void rend_set_thread_id(std::thread::id id);
std::thread::id rend_get_thread_id();
bool is_rend_thread();

void sim_set_is_running(bool is_running);
bool rend_get_is_running();

} // namespace Mizu