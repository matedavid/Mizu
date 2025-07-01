#include "base_state_manager.h"

#include "base/debug/assert.h"

namespace Mizu
{

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager<StaticState, DynamicState, Handle, Config>::BaseStateManager()
{
    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        m_available_handles.push(id);
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager<StaticState, DynamicState, Handle, Config>::~BaseStateManager()
{
}

//
// Sim side functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_begin_tick()
{
    const Fence& fence = m_in_flight_fences[m_sim_pos];
    while (!fence.is_open.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        const uint32_t prev = get_prev_pos(m_sim_pos);
        m_handles_dynamic_state[id + static_cast<uint64_t>(m_sim_pos)] =
            m_handles_dynamic_state[id + static_cast<uint64_t>(prev)];
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_end_tick()
{
    for (auto it = m_requested_releases_map.begin(); it != m_requested_releases_map.end();)
    {
        const uint64_t id = it->first;
        const bool acknowledged = it->second;

        if (acknowledged)
        {
            m_available_handles.push(id);
            it = m_requested_releases_map.erase(it);
        }
        else
        {
            it = std::next(it);
        }
    }

    m_in_flight_fences[m_sim_pos].is_open.store(false, std::memory_order_relaxed);
    m_sim_pos = get_next_pos(m_sim_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
Handle BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_create_handle(StaticState static_state,
                                                                                      DynamicState dynamic_state)
{
    MIZU_ASSERT(!m_available_handles.empty(), "There are no available handles");

    const uint64_t id = m_available_handles.top();
    m_available_handles.pop();

    m_handles_static_state[id] = static_state;
    m_handles_dynamic_state[id + static_cast<uint64_t>(m_sim_pos)] = dynamic_state;

    return Handle(id);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_release_handle(Handle handle)
{
    sim_mark_handle_for_release(handle.get_internal_id());
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_update(Handle handle, DynamicState dynamic_state)
{
    const uint64_t id = handle.get_internal_id();
    m_handles_dynamic_state[id + static_cast<uint64_t>(m_sim_pos)] = dynamic_state;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
StaticState BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_static_state(Handle handle) const
{
    return m_handles_static_state[handle.get_internal_id()];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_dynamic_state(Handle handle) const
{
    const uint64_t id = handle.get_internal_id();
    return m_handles_dynamic_state[id + static_cast<uint64_t>(m_sim_pos)];
}

//
// Render side functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_begin_frame()
{
    const Fence& fence = m_in_flight_fences[m_rend_pos];
    while (fence.is_open.load(std::memory_order_relaxed))
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_end_frame()
{
    for (const auto& [id, acknowledged] : m_requested_releases_map)
    {
        if (!acknowledged)
        {
            rend_acknowledge_handle_release(id);
        }
    }

    m_in_flight_fences[m_rend_pos].is_open.store(true, std::memory_order_relaxed);
    m_rend_pos = get_next_pos(m_rend_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
StaticState BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_static_state(Handle handle) const
{
    // TODO: Probably not a good idea
    return sim_get_static_state(handle);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_dynamic_state(Handle handle) const
{
    const uint64_t id = handle.get_internal_id();
    return m_handles_dynamic_state[id + static_cast<uint64_t>(m_rend_pos)];
}

//
// Helpers
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_mark_handle_for_release(uint64_t id)
{
    m_requested_releases_map.insert({id, false});
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_acknowledge_handle_release(uint64_t id)
{
    auto it = m_requested_releases_map.find(id);
    MIZU_ASSERT(it != m_requested_releases_map.end(), "Handle with id {} has not been requested for release", id);

    it->second = true;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
uint32_t BaseStateManager<StaticState, DynamicState, Handle, Config>::get_next_pos(uint32_t pos)
{
    return (pos + 1) % Config::MaxStatesInFlight;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
uint32_t BaseStateManager<StaticState, DynamicState, Handle, Config>::get_prev_pos(uint32_t pos)
{
    if (pos == 0)
        return Config::MaxStatesInFlight - 1;

    return pos - 1;
}

} // namespace Mizu