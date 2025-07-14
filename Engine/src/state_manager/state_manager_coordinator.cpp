#include "state_manager_coordinator.h"

#include <type_traits>

#include "state_manager/camera_state_manager.h"
#include "state_manager/light_state_manager.h"
#include "state_manager/static_mesh_state_manager.h"
#include "state_manager/transform_state_manager.h"

namespace Mizu
{

#define MIZU_STATE_MANAGERS_LIST \
    X(g_transform_state_manager) X(g_static_mesh_state_manager) X(g_light_state_manager) X(g_camera_state_manager)

StateManagerCoordinator::StateManagerCoordinator()
{
#define X(_state_manager) _state_manager = new std::remove_pointer_t<decltype(_state_manager)>();
    MIZU_STATE_MANAGERS_LIST
#undef X
}

StateManagerCoordinator::~StateManagerCoordinator()
{
#define X(_state_manager) delete _state_manager;
    MIZU_STATE_MANAGERS_LIST
#undef X
}

void StateManagerCoordinator::sim_begin_tick()
{
#define X(_state_manager) _state_manager->sim_begin_tick();
    MIZU_STATE_MANAGERS_LIST
#undef X
}

void StateManagerCoordinator::sim_end_tick()
{
#define X(_state_manager) _state_manager->sim_end_tick();
    MIZU_STATE_MANAGERS_LIST
#undef X
}

void StateManagerCoordinator::rend_begin_frame()
{
#define X(_state_manager) _state_manager->rend_begin_frame();
    MIZU_STATE_MANAGERS_LIST
#undef X
}

void StateManagerCoordinator::rend_end_frame()
{
#define X(_state_manager) _state_manager->rend_end_frame();
    MIZU_STATE_MANAGERS_LIST
#undef X
}

} // namespace Mizu
