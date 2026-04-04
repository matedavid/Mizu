#include "state_manager/state_manager_coordinator.h"

#include <queue>

#include "base/debug/logging.h"
#include "base/utils/hash.h"

namespace Mizu
{

//
// StateManagerRegistrationBuilder
//

StateManagerRegistrationBuilder StateManagerRegistrationBuilder::begin(IStateManager* state_manager)
{
    return StateManagerRegistrationBuilder(state_manager);
}

StateManagerRegistrationBuilder& StateManagerRegistrationBuilder::depends_on(IStateManager* depends_on)
{
    append_if_not_present(m_inputs, depends_on);
    return *this;
}

StateManagerRegistrationBuilder& StateManagerRegistrationBuilder::required_by(IStateManager* required_by)
{
    append_if_not_present(m_outputs, required_by);
    return *this;
}

void StateManagerRegistrationBuilder::append_if_not_present(
    inplace_vector<IStateManager*, MAX_STATE_MANAGER_DEPENDENCIES>& vector,
    IStateManager* manager)
{
    for (IStateManager* sm : vector)
    {
        if (sm->get_identifier() == manager->get_identifier())
            return;
    }

    vector.push_back(manager);
}

//
// StateManagerCoordinator
//

void StateManagerCoordinator2::register_state_manager(const StateManagerRegistrationBuilder& builder)
{
    if (builder.m_state_manager->get_identifier() == "BaseStateManager")
    {
        MIZU_LOG_WARNING(
            "Trying to register StateManager with default identifier 'BaseStateManager', could cause conflicts");
    }

    const size_t identifier_hash = hash_compute(builder.m_state_manager->get_identifier());
    if (m_state_manager_to_infos_pos.contains(identifier_hash))
    {
        MIZU_LOG_WARNING(
            "State manager with identifier '{}' is already registered, skipping",
            builder.m_state_manager->get_identifier());

        return;
    }

    StateManagerInfo info{};
    info.state_manager = builder.m_state_manager;

    const size_t new_state_manager_idx = m_state_manager_infos.size();

    const auto push_unique = [](inplace_vector<size_t, MAX_STATE_MANAGER_DEPENDENCIES>& vector, size_t value) {
        for (size_t existing : vector)
        {
            if (existing == value)
                return;
        }

        vector.push_back(value);
    };

    for (const IStateManager* input : builder.m_inputs)
    {
        const size_t input_identifier_hash = hash_compute(input->get_identifier());
        const auto it = m_state_manager_to_infos_pos.find(input_identifier_hash);

        if (it == m_state_manager_to_infos_pos.end())
        {
            MIZU_LOG_ERROR(
                "Input state manager with identifier '{}' is not registered, skipping dependency",
                input->get_identifier());

            continue;
        }

        const size_t input_idx = it->second;
        push_unique(info.inputs, input_idx);
        push_unique(m_state_manager_infos[input_idx].outputs, new_state_manager_idx);
    }

    for (const IStateManager* output : builder.m_outputs)
    {
        const size_t output_identifier_hash = hash_compute(output->get_identifier());
        const auto it = m_state_manager_to_infos_pos.find(output_identifier_hash);

        if (it == m_state_manager_to_infos_pos.end())
        {
            MIZU_LOG_ERROR(
                "Output state manager with identifier '{}' is not registered, skipping dependency",
                output->get_identifier());
            continue;
        }

        const size_t output_idx = it->second;
        push_unique(info.outputs, output_idx);
        push_unique(m_state_manager_infos[output_idx].inputs, new_state_manager_idx);
    }

    m_state_manager_infos.push_back(info);
    m_state_manager_to_infos_pos.insert({identifier_hash, new_state_manager_idx});

    // Probably overkill, but as registering is probably only done at startup I believe it's safer to do it here
    build();
}

void StateManagerCoordinator2::sim_begin_tick(const TickUpdateState& state) const
{
    for (IStateManager* manager : m_state_managers)
        manager->sim_begin_tick(state);
}

void StateManagerCoordinator2::sim_end_tick() const
{
    for (IStateManager* manager : m_state_managers)
        manager->sim_end_tick();
}

void StateManagerCoordinator2::rend_apply_updates(const FrameUpdateState& state) const
{
    for (IStateManager* manager : m_state_managers)
        manager->rend_apply_updates(state);
}

void StateManagerCoordinator2::build()
{
    m_state_managers.clear();

    std::vector<uint32_t> in_degree(m_state_manager_infos.size(), 0);

    for (size_t i = 0; i < m_state_manager_infos.size(); ++i)
    {
        const StateManagerInfo& info = m_state_manager_infos[i];
        in_degree[i] = info.inputs.size();
    }

    std::queue<size_t> priority_queue;
    for (size_t i = 0; i < in_degree.size(); ++i)
    {
        if (in_degree[i] == 0)
            priority_queue.push(i);
    }

    while (!priority_queue.empty())
    {
        const size_t top = priority_queue.front();
        priority_queue.pop();

        m_state_managers.push_back(m_state_manager_infos[top].state_manager);

        for (size_t adj : m_state_manager_infos[top].outputs)
        {
            if (--in_degree[adj] == 0)
            {
                priority_queue.push(adj);
            }
        }
    }

    MIZU_ASSERT(
        m_state_managers.size() == m_state_manager_infos.size(), "A cycle was detected in StateManagerCoordinator");
}

} // namespace Mizu