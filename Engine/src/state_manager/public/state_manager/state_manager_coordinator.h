#pragma once

#include <unordered_map>
#include <vector>

#include "base/containers/inplace_vector.h"

#include "mizu_state_manager_module.h"
#include "state_manager/state_manager.h"

namespace Mizu
{

struct TickUpdateState;
struct FrameUpdateState;

constexpr size_t MAX_STATE_MANAGER_DEPENDENCIES = 10;

class MIZU_STATE_MANAGER_API StateManagerRegistrationBuilder
{
  public:
    static StateManagerRegistrationBuilder begin(IStateManager* state_manager);

    // The state manager being registered depends on the state manager passed in
    StateManagerRegistrationBuilder& depends_on(IStateManager* depends_on);

    // The state manager being registered is required by the state manager passed in
    StateManagerRegistrationBuilder& required_by(IStateManager* required_by);

  private:
    StateManagerRegistrationBuilder(IStateManager* state_manager) : m_state_manager(state_manager) {}

    IStateManager* m_state_manager;

    inplace_vector<IStateManager*, MAX_STATE_MANAGER_DEPENDENCIES> m_inputs;
    inplace_vector<IStateManager*, MAX_STATE_MANAGER_DEPENDENCIES> m_outputs;

    static void append_if_not_present(
        inplace_vector<IStateManager*, MAX_STATE_MANAGER_DEPENDENCIES>& vector,
        IStateManager* manager);

    friend class StateManagerCoordinator2;
};

class MIZU_STATE_MANAGER_API StateManagerCoordinator2
{
  public:
    StateManagerCoordinator2() = default;

    void register_state_manager(const StateManagerRegistrationBuilder& builder);

    void sim_begin_tick(const TickUpdateState& state) const;
    void sim_end_tick() const;

    void rend_apply_updates(const FrameUpdateState& state) const;

    void build();

  private:
    struct StateManagerInfo
    {
        IStateManager* state_manager;

        inplace_vector<size_t, MAX_STATE_MANAGER_DEPENDENCIES> inputs;
        inplace_vector<size_t, MAX_STATE_MANAGER_DEPENDENCIES> outputs;
    };

    std::vector<IStateManager*> m_state_managers;
    std::vector<StateManagerInfo> m_state_manager_infos;

    std::unordered_map<size_t, size_t> m_state_manager_to_infos_pos;
};

} // namespace Mizu