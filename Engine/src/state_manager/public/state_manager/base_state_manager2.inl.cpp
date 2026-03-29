#include "state_manager/base_state_manager2.h"

#include "base/debug/assert.h"
#include "base_state_manager2.h"

namespace Mizu
{

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager2<StaticState, DynamicState, Handle, Config>::BaseStateManager2()
{
    for (uint64_t i = 0; i < Config::MaxNumHandles; ++i)
    {
        m_available_handles.push(i);
        m_pending_handles_idx[i] = INVALID_PENDING_IDX;
    }
}

//
// Sim functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_begin_tick()
{
    const uint64_t last_produced_tick = m_last_produced_tick.load(std::memory_order_relaxed);
    const uint64_t last_consumed_tick = m_last_consumed_tick.load(std::memory_order_relaxed);

    const uint64_t tick_difference = last_produced_tick - last_consumed_tick;

    if (tick_difference >= MaxTicksAhead)
    {
        // The render can't consume the ticks fast enough, wait until render thread finishes consuming one tick.
        // TODO: Should maybe be a warning?

        // TODO: IMPLEMENT
    }

    const uint64_t tick_in_production_ring_idx = (last_produced_tick + 1) % MaxTicksAhead;
    m_tick_in_production = &m_ticks[tick_in_production_ring_idx];

    *m_tick_in_production = Tick{};
    m_tick_in_production->tick_idx = m_last_produced_tick + 1;
    m_tick_in_production->num_updated_handles = 0;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_end_tick()
{
    // Don't produce a new tick if the current tick in production has no updates
    if (m_tick_in_production->num_updated_handles == 0)
    {
        m_tick_in_production = nullptr;
        return;
    }

    const uint64_t tick_ring_idx = (m_tick_in_production->tick_idx % MaxTicksAhead) * Config::MaxNumHandles;
    for (uint64_t i = 0; i < m_tick_in_production->num_updated_handles; ++i)
    {
        const HandleTick& ht = m_handle_ticks[tick_ring_idx + i];
        m_pending_handles_idx[ht.handle.get_internal_id()] = INVALID_PENDING_IDX;
    }

    m_last_produced_tick.store(m_tick_in_production->tick_idx, std::memory_order_release);
    m_tick_in_production = nullptr;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
Handle BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_create(
    StaticState static_state,
    DynamicState dynamic_state)
{
    if (m_available_handles.empty())
    {
        MIZU_UNREACHABLE("Trying to create more handles than MaxNumHandles");
        return Handle{};
    }

    const Handle handle = m_available_handles.top();
    m_available_handles.pop();

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.handle = handle;
    handle_tick.event_kind = StateManagerEventKind::Create;
    handle_tick.ss = static_state;
    handle_tick.ds = dynamic_state;

    return handle;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_destroy(Handle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");

    const uint64_t handle_idx = handle.get_internal_id();

    if (m_pending_handles_idx[handle_idx] != INVALID_PENDING_IDX)
    {
        const uint32_t pending_handle_idx = m_pending_handles_idx[handle_idx];
        HandleTick& handle_tick = m_handle_ticks[pending_handle_idx];

        // Destroy after Create drops the event
        if (handle_tick.event_kind == StateManagerEventKind::Create)
        {
            const uint64_t tick_ring_start_idx =
                (m_tick_in_production->tick_idx % MaxTicksAhead) * Config::MaxNumHandles;

            auto start_handle_ticks = std::next(m_handle_ticks.begin(), tick_ring_start_idx);
            auto end_handle_ticks = std::next(start_handle_ticks, m_tick_in_production->num_updated_handles);

            const auto new_end_it = std::remove_if(
                start_handle_ticks, end_handle_ticks, [&](const HandleTick& ht) { return ht.handle == handle; });

            if (new_end_it != end_handle_ticks)
                *new_end_it = HandleTick{};

            m_tick_in_production->num_updated_handles -= 1;
            m_pending_handles_idx[handle_idx] = INVALID_PENDING_IDX;

            // Update pending indices for all handles that shifted left after compaction
            const uint64_t removed_local_idx = pending_handle_idx - tick_ring_start_idx;
            for (uint64_t i = removed_local_idx; i < m_tick_in_production->num_updated_handles; ++i)
            {
                const HandleTick& shifted_ht = m_handle_ticks[tick_ring_start_idx + i];
                m_pending_handles_idx[shifted_ht.handle.get_internal_id()] = tick_ring_start_idx + i;
            }

            m_available_handles.push(handle.get_internal_id());
        }
        else
        {
            // In the rest of cases, any kind converts to destroy
            handle_tick.event_kind = StateManagerEventKind::Destroy;
        }

        return;
    }

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Destroy;
    handle_tick.ss = StaticState{};
    handle_tick.ds = DynamicState{};
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_update(Handle handle, DynamicState dynamic_state)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");

    // TODO: Implement only updating if the new dynamic state is different from the previous value with
    // DynamicState::has_changed

    const uint64_t handle_idx = handle.get_internal_id();

    if (m_pending_handles_idx[handle_idx] != INVALID_PENDING_IDX)
    {
        const uint32_t pending_handle_idx = m_pending_handles_idx[handle_idx];
        HandleTick& handle_tick = m_handle_ticks[pending_handle_idx];

        // If it already exits, the event kind does not matter, just update the dynamic state.
        // - If it's a create event, the new dynamic state is the initial dynamic state
        // - If it's a update event, the new dynamic state is the new dynamic state
        // - If it's a destroy event, the dynamic state will not get used
        handle_tick.ds = dynamic_state;

        return;
    }

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Update;
    handle_tick.ds = dynamic_state;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState& BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_edit(Handle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");

    const uint64_t handle_idx = handle.get_internal_id();

    if (m_pending_handles_idx[handle_idx] != INVALID_PENDING_IDX)
    {
        const uint32_t pending_handle_idx = m_pending_handles_idx[handle_idx];
        HandleTick& handle_tick = m_handle_ticks[pending_handle_idx];

        return handle_tick.ds;
    }

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Update;

    return handle_tick.ds;
}

//
// Rend functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_begin_frame()
{
}

//
// Helpers
//

#define HandleTickCpp BaseStateManager2<StaticState, DynamicState, Handle, Config>::HandleTick

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
HandleTickCpp& BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_allocate_handle_tick(Handle handle)
{
    MIZU_ASSERT(m_tick_in_production != nullptr, "There is no tick in production");

    const uint64_t tick_ring_idx = (m_tick_in_production->tick_idx % MaxTicksAhead) * Config::MaxNumHandles
                                   + m_tick_in_production->num_updated_handles;

    HandleTick& handle_tick = m_handle_ticks[tick_ring_idx];
    handle_tick = HandleTick{};
    handle_tick.handle = handle;

    m_tick_in_production->num_updated_handles += 1;
    m_pending_handles_idx[handle.get_internal_id()] = tick_ring_idx;

    return handle_tick;
}

} // namespace Mizu
