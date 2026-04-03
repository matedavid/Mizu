#include "state_manager/base_state_manager2.h"

#include <algorithm>

#include "base/debug/assert.h"

namespace Mizu
{

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
BaseStateManager2<StaticState, DynamicState, Handle, Config>::BaseStateManager2()
{
    for (uint64_t i = 0; i < Config::MaxNumHandles; ++i)
    {
        m_available_handles.push(i);
        m_pending_handles_idx[i] = INVALID_PENDING_IDX;
        m_rend_dynamic_state_info[i] = RendDynamicStateInfo{};

        for (uint64_t tick_idx = 0; tick_idx < MaxTicksAhead; ++tick_idx)
        {
            const uint64_t handle_tick_idx = tick_idx * Config::MaxNumHandles + i;
            m_handle_ticks[handle_tick_idx] = HandleTick{};
        }
    }
}

//
// Sim functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_begin_tick(const TickUpdateState& state)
{
    const uint64_t last_produced_tick = m_last_produced_tick.load(std::memory_order_relaxed);
    const uint64_t last_consumed_tick = m_last_consumed_tick.load(std::memory_order_acquire);

    // Reclaim handles from destroy events in ticks that render has fully consumed
    for (uint64_t tick_idx = m_last_reclaimed_tick + 1; tick_idx <= last_consumed_tick; ++tick_idx)
    {
        const Tick& tick = m_ticks[tick_idx % MaxTicksAhead];
        const uint64_t slot_base = (tick_idx % MaxTicksAhead) * Config::MaxNumHandles;

        for (uint32_t i = 0; i < tick.num_updated_handles; ++i)
        {
            const HandleTick& handle_tick = m_handle_ticks[slot_base + i];
            if (handle_tick.event_kind == StateManagerEventKind::Destroy)
            {
                m_available_handles.push(handle_tick.handle.get_internal_id());
                m_handle_static_states[handle_tick.handle.get_internal_id()] = StaticState{};
            }
        }
    }
    m_last_reclaimed_tick = last_consumed_tick;

    const uint64_t tick_difference = last_produced_tick - last_consumed_tick;

    if (tick_difference >= MaxTicksAhead)
    {
        // If render can't consume the ticks fast enough, wait until render thread finishes consuming one tick.

        std::unique_lock lock(m_tick_consumed_mutex);
        m_tick_consumed_cv.wait(lock, [&]() {
            const uint64_t last_consumed = m_last_consumed_tick.load(std::memory_order_acquire);
            return (last_produced_tick - last_consumed) < MaxTicksAhead;
        });
    }

    const uint64_t tick_in_production_ring_idx = (last_produced_tick + 1) % MaxTicksAhead;
    m_tick_in_production = &m_ticks[tick_in_production_ring_idx];

    *m_tick_in_production = Tick{};
    m_tick_in_production->tick_idx = m_last_produced_tick + 1;
    m_tick_in_production->sim_time_us = state.sim_time_us;
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

    m_handle_static_states[handle.get_internal_id()] = static_state;

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Create;
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
                m_pending_handles_idx[shifted_ht.handle.get_internal_id()] =
                    static_cast<uint32_t>(tick_ring_start_idx + i);
            }

            m_handle_static_states[handle.get_internal_id()] = StaticState{};
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
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_apply_updates(const FrameUpdateState& state)
{
    const uint64_t last_produced_tick_idx = m_last_produced_tick.load(std::memory_order_acquire);
    const uint64_t last_consumed_tick_idx = m_last_consumed_tick.load(std::memory_order_relaxed);

    if (last_produced_tick_idx == last_consumed_tick_idx)
    {
        // No new tick produced, nothing to consume
        return;
    }

    const Tick& last_consumed_tick = m_ticks[last_consumed_tick_idx % MaxTicksAhead];

    const uint64_t tick_to_consume_idx = last_consumed_tick_idx + 1;
    const Tick& tick_to_consume = m_ticks[tick_to_consume_idx % MaxTicksAhead];

    const uint64_t t0 = last_consumed_tick.sim_time_us;
    const uint64_t t1 = tick_to_consume.sim_time_us;

    double alpha = 1.0;
    if (t1 > t0)
    {
        const double numerator = static_cast<double>(state.render_time_us - t0);
        const double denominator = static_cast<double>(t1 - t0);

        alpha = std::clamp(numerator / denominator, 0.0, 1.0);
    }

    const bool fully_consumed = alpha >= 1.0;

    for (uint64_t i = 0; i < tick_to_consume.num_updated_handles; ++i)
    {
        const uint64_t handle_tick_idx = (tick_to_consume.tick_idx % MaxTicksAhead) * Config::MaxNumHandles + i;
        const HandleTick& handle_tick = m_handle_ticks[handle_tick_idx];

        RendDynamicStateInfo& last_consumed_rend_info = m_rend_dynamic_state_info[handle_tick.handle.get_internal_id()];

        switch (handle_tick.event_kind)
        {
        case StateManagerEventKind::Create: {
            if (fully_consumed)
            {
                const StaticState& ss = m_handle_static_states[handle_tick.handle.get_internal_id()];
                rend_on_create(handle_tick.handle, ss, handle_tick.ds);
            }

            break;
        }
        case StateManagerEventKind::Update: {
            if (fully_consumed)
            {
                rend_on_update(handle_tick.handle, handle_tick.ds);
            }
            else if (Config::Interpolate)
            {
                const DynamicState interpolated_ds =
                    last_consumed_rend_info.consumed_ds.interpolate(handle_tick.ds, alpha);
                rend_on_update(handle_tick.handle, interpolated_ds);

                last_consumed_rend_info.applied_ds = interpolated_ds;
            }

            break;
        }
        case StateManagerEventKind::Destroy: {
            if (fully_consumed)
            {
                rend_on_destroy(handle_tick.handle);
            }

            break;
        }
        }

        if (fully_consumed)
        {
            last_consumed_rend_info.consumed_ds = handle_tick.ds;
            last_consumed_rend_info.applied_ds = handle_tick.ds;
        }
    }

    if (fully_consumed)
    {
        m_last_consumed_tick.store(tick_to_consume_idx, std::memory_order_release);
        m_tick_consumed_cv.notify_one();
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_get_dynamic_state(
    Handle handle) const
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");
    return m_rend_dynamic_state_info[handle.get_internal_id()].applied_ds;
}

//
// Other functions
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const StaticState& BaseStateManager2<StaticState, DynamicState, Handle, Config>::get_static_state(Handle handle) const
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");
    return m_handle_static_states[handle.get_internal_id()];
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
    m_pending_handles_idx[handle.get_internal_id()] = static_cast<uint32_t>(tick_ring_idx);

    return handle_tick;
}

} // namespace Mizu
