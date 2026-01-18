#pragma once

#include "base_state_manager.h"

#include "base/debug/assert.h"
#include "base/debug/profiling.h"
#include "core/thread_sync.h"

namespace Mizu
{

#define IteratorWrapperCpp BaseStateManager<StaticState, DynamicState, Handle, Config>::IteratorWrapper

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager<StaticState, DynamicState, Handle, Config>::BaseStateManager()
{
    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        m_available_handles.push(id);
    }

    m_active_handles.reserve(Config::MaxNumHandles);

    // By default, all in flight fences are signaled so that the sim tick executes before the rend frame
    m_in_flight_fences.fill(ThreadFence(true));
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
    MIZU_PROFILE_SCOPED;

    const ThreadFence& fence = m_in_flight_fences[m_sim_pos];
    fence.wait_signaled();

    for (uint64_t id = 0; id < Config::MaxNumHandles; ++id)
    {
        const size_t prev = get_prev_pos(m_sim_pos);
        edit_dynamic_state_internal(Handle(id), m_sim_pos) = get_dynamic_state_internal(Handle(id), prev);
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_end_tick()
{
    MIZU_PROFILE_SCOPED;

    for (auto it = m_requested_releases_map.begin(); it != m_requested_releases_map.end();)
    {
        const uint64_t id = it->first;
        const bool acknowledged = it->second;

        if (acknowledged)
        {
            m_available_handles.push(id);
            it = m_requested_releases_map.erase(it);

            const auto ah_it = std::find(m_active_handles.begin(), m_active_handles.end(), id);
            m_active_handles.erase(ah_it);

            // Reset static and dynamic state to default values
            m_handles_static_state[id] = StaticState{};
            for (uint32_t i = 0; i < Config::MaxStatesInFlight; ++i)
                edit_dynamic_state_internal(Handle(id), i) = DynamicState{};
        }
        else
        {
            it = std::next(it);
        }
    }

    m_in_flight_fences[m_sim_pos].reset();
    m_sim_pos = get_next_pos(m_sim_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
Handle BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_create_handle(
    StaticState static_state,
    DynamicState dynamic_state)
{
    MIZU_ASSERT(!m_available_handles.empty(), "There are no available handles");

    const uint64_t id = m_available_handles.top();
    m_available_handles.pop();

    m_active_handles.push_back(id);

    m_handles_static_state[id] = static_state;
    for (uint32_t p = 0; p < Config::MaxStatesInFlight; ++p)
        edit_dynamic_state_internal(Handle(id), p) = dynamic_state;

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
#if MIZU_DEBUG
    validate_handle_not_marked_for_release(handle);
#endif

    edit_dynamic_state_internal(handle, m_sim_pos) = dynamic_state;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const StaticState& BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_static_state(
    Handle handle) const
{
#if MIZU_DEBUG
    validate_handle_not_marked_for_release(handle);
#endif

    return m_handles_static_state[handle.get_internal_id()];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_get_dynamic_state(
    Handle handle) const
{
#if MIZU_DEBUG
    validate_handle_not_marked_for_release(handle);
#endif

    return get_dynamic_state_internal(handle, m_sim_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::sim_edit_dynamic_state(Handle handle)
{
#if MIZU_DEBUG
    validate_handle_not_marked_for_release(handle);
#endif

    return edit_dynamic_state_internal(handle, m_sim_pos);
}

//
// Render side functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_begin_frame()
{
    MIZU_PROFILE_SCOPED;

    const ThreadFence& fence = m_in_flight_fences[m_rend_pos];
    fence.wait_not_signaled();
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_end_frame()
{
    MIZU_PROFILE_SCOPED;

    for (const auto& [id, acknowledged] : m_requested_releases_map)
    {
        if (!acknowledged)
        {
            rend_acknowledge_handle_release(id);
        }
    }

    m_in_flight_fences[m_rend_pos].signal();
    m_rend_pos = get_next_pos(m_rend_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const StaticState& BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_static_state(
    Handle handle) const
{
    return m_handles_static_state[handle.get_internal_id()];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_get_dynamic_state(
    Handle handle) const
{
    const uint64_t id = handle.get_internal_id();
    return get_dynamic_state_internal(id, m_rend_pos);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
IteratorWrapperCpp BaseStateManager<StaticState, DynamicState, Handle, Config>::rend_iterator()
{
    return IteratorWrapper(m_active_handles);
}

//
// General functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const StaticState& BaseStateManager<StaticState, DynamicState, Handle, Config>::get_static_state(Handle handle) const
{
    // TODO: Check this is correct, when getting static state it doesn't matter if it's render or sim side
    return sim_get_static_state(handle);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::get_dynamic_state(Handle handle) const
{
    // TODO: Check this is correct, getting dynamic state is probably render side operation
    return rend_get_dynamic_state(handle);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::edit_dynamic_state(Handle handle)
{
    // TODO: Check this is correct, getting dynamic state is exclusively simulation side operation
    return sim_edit_dynamic_state(handle);
}

#undef RendIteratorWrapperCpp

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
size_t BaseStateManager<StaticState, DynamicState, Handle, Config>::get_next_pos(size_t pos)
{
    return (pos + 1) % Config::MaxStatesInFlight;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
size_t BaseStateManager<StaticState, DynamicState, Handle, Config>::get_prev_pos(size_t pos)
{
    if (pos == 0)
        return Config::MaxStatesInFlight - 1;

    return pos - 1;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::get_dynamic_state_internal(
    Handle handle,
    size_t pos) const
{
    MIZU_ASSERT(pos >= 0 && pos < Config::MaxStatesInFlight, "Invalid pos value {}", pos);

    const uint64_t id = handle.get_internal_id();
    return m_handles_dynamic_state[id * Config::MaxStatesInFlight + static_cast<uint64_t>(pos)];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState& BaseStateManager<StaticState, DynamicState, Handle, Config>::edit_dynamic_state_internal(
    Handle handle,
    size_t pos)
{
    MIZU_ASSERT(pos >= 0 && pos < Config::MaxStatesInFlight, "Invalid pos value {}", pos);

    const uint64_t id = handle.get_internal_id();
    return m_handles_dynamic_state[id * Config::MaxStatesInFlight + static_cast<uint64_t>(pos)];
}

#if MIZU_DEBUG

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager<StaticState, DynamicState, Handle, Config>::validate_handle_not_marked_for_release(
    Handle handle) const
{
    const auto it = m_requested_releases_map.find(handle.get_internal_id());
    MIZU_ASSERT(
        it == m_requested_releases_map.end(),
        "Handle {} has been marked for release, it should not be used again",
        handle.get_internal_id());
}

#endif

} // namespace Mizu