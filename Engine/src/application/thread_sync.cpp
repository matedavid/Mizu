#include "thread_sync.h"

#include "base/debug/assert.h"

namespace Mizu
{

static std::thread::id s_sim_thread_id;
static std::thread::id s_rend_thread_id;

static bool s_is_running = true;

void sim_set_thread_id(std::thread::id id)
{
    s_sim_thread_id = id;
}

std::thread::id sim_get_thread_id()
{
    MIZU_ASSERT(s_sim_thread_id != std::thread::id(), "Sim thread id has not been set, call sim_set_thread_id");

    return s_sim_thread_id;
}

bool is_sim_thread()
{
    return std::this_thread::get_id() == sim_get_thread_id();
}

void rend_set_thread_id(std::thread::id id)
{
    s_rend_thread_id = id;
}

std::thread::id rend_get_thread_id()
{
    MIZU_ASSERT(s_rend_thread_id != std::thread::id(), "Rend thread id has not been set, call rend_set_thread_id");

    return s_rend_thread_id;
}

bool is_rend_thread()
{
    return std::this_thread::get_id() == rend_get_thread_id();
}

void sim_set_is_running(bool is_running)
{
    s_is_running = is_running;
}

bool rend_get_is_running()
{
    return s_is_running;
}

} // namespace Mizu