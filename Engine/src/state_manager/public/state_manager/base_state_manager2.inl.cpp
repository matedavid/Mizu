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
        m_handle_state_info[i] = HandleStateInfo{};

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
            HandleTick& handle_tick = m_handle_ticks[slot_base + i];
            if (handle_tick.event_kind == StateManagerEventKind::Destroy)
                sim_reset_and_recycle_handle(handle_tick.handle, &handle_tick);
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

        if (ht.event_kind != StateManagerEventKind::Destroy)
            m_handle_state_info[ht.handle.get_internal_id()].sim_published_ds = ht.ds;

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
    m_handle_state_info[handle.get_internal_id()] = HandleStateInfo{};
    m_handle_state_info[handle.get_internal_id()].sim_alive = true;

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

        if (handle_tick.event_kind == StateManagerEventKind::Destroy)
        {
            MIZU_ASSERT(false, "Trying to destroy a handle that is already destroyed on sim side");
            return;
        }

        // Destroy after Create drops the event
        if (handle_tick.event_kind == StateManagerEventKind::Create)
        {
            const uint64_t tick_ring_start_idx =
                (m_tick_in_production->tick_idx % MaxTicksAhead) * Config::MaxNumHandles;

            auto start_handle_ticks = std::next(m_handle_ticks.begin(), tick_ring_start_idx);
            auto end_handle_ticks = std::next(start_handle_ticks, m_tick_in_production->num_updated_handles);

            const auto new_end_it = std::remove_if(
                start_handle_ticks, end_handle_ticks, [&](const HandleTick& ht) { return ht.handle == handle; });

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

            sim_reset_and_recycle_handle(handle, new_end_it != end_handle_ticks ? &(*new_end_it) : nullptr);
        }
        else
        {
            // In the rest of cases, any kind converts to destroy
            handle_tick.event_kind = StateManagerEventKind::Destroy;
            m_handle_state_info[handle_idx].sim_alive = false;
        }

        return;
    }

    sim_validate_handle_is_alive(handle);

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Destroy;
    handle_tick.ds = DynamicState{};

    m_handle_state_info[handle_idx].sim_alive = false;
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

        if (handle_tick.event_kind == StateManagerEventKind::Destroy)
        {
            MIZU_UNREACHABLE("Trying to update a handle that is destroyed on sim side");
            return;
        }

        // If it already exits, the event kind does not matter, just update the dynamic state.
        // - If it's a create event, the new dynamic state is the initial dynamic state
        // - If it's a update event, the new dynamic state is the new dynamic state
        // - If it's a destroy event, the dynamic state will not get used
        handle_tick.ds = dynamic_state;

        return;
    }

    sim_validate_handle_is_alive(handle);

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Update;
    handle_tick.ds = dynamic_state;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const DynamicState& BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_get_dynamic_state(
    Handle handle) const
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");

    if (const HandleTick* pending_handle_tick = sim_get_pending_handle_tick(handle))
    {
        MIZU_ASSERT(
            pending_handle_tick->event_kind != StateManagerEventKind::Destroy,
            "Trying to get the dynamic state of a handle that is destroyed on sim side");
        return pending_handle_tick->ds;
    }

    sim_validate_handle_is_alive(handle);
    return m_handle_state_info[handle.get_internal_id()].sim_published_ds;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
DynamicState& BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_edit(Handle handle)
{
    MIZU_ASSERT(handle.is_valid(), "Invalid handle");

    if (HandleTick* pending_handle_tick = sim_get_pending_handle_tick(handle))
    {
        MIZU_ASSERT(
            pending_handle_tick->event_kind != StateManagerEventKind::Destroy,
            "Trying to edit the dynamic state of a handle that is destroyed on sim side");

        return pending_handle_tick->ds;
    }

    sim_validate_handle_is_alive(handle);

    HandleTick& handle_tick = sim_allocate_handle_tick(handle);
    handle_tick.event_kind = StateManagerEventKind::Update;
    handle_tick.ds = m_handle_state_info[handle.get_internal_id()].sim_published_ds;

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

        HandleStateInfo& handle_state_info = m_handle_state_info[handle_tick.handle.get_internal_id()];

        switch (handle_tick.event_kind)
        {
        case StateManagerEventKind::Create: {
            if (fully_consumed)
            {
                const StaticState& ss = m_handle_static_states[handle_tick.handle.get_internal_id()];

                rend_notify_on_create(handle_tick.handle, ss, handle_tick.ds);
            }

            break;
        }
        case StateManagerEventKind::Update: {
            if (fully_consumed)
            {
                rend_notify_on_update(handle_tick.handle, handle_tick.ds);
            }
            else if (Config::Interpolate)
            {
                const DynamicState interpolated_ds = handle_state_info.consumed_ds.interpolate(handle_tick.ds, alpha);
                rend_notify_on_update(handle_tick.handle, interpolated_ds);

                handle_state_info.applied_ds = interpolated_ds;
            }

            break;
        }
        case StateManagerEventKind::Destroy: {
            if (fully_consumed)
            {
                rend_notify_on_destroy(handle_tick.handle);
            }

            break;
        }
        }

        if (fully_consumed)
        {
            handle_state_info.consumed_ds = handle_tick.ds;
            handle_state_info.applied_ds = handle_tick.ds;
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
    return m_handle_state_info[handle.get_internal_id()].applied_ds;
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::register_rend_consumer(
    IStateManagerConsumer<SelfStateManager>* listener)
{
    m_rend_consumers.push_back(listener);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::unregister_rend_consumer(
    IStateManagerConsumer<SelfStateManager>* listener)
{
    m_rend_consumers.erase(
        std::remove(m_rend_consumers.begin(), m_rend_consumers.end(), listener), m_rend_consumers.end());
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

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
std::string_view BaseStateManager2<StaticState, DynamicState, Handle, Config>::get_identifier() const
{
    return Config::Identifier;
}

//
// Helpers
//

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_notify_on_create(
    Handle handle,
    const StaticState& ss,
    const DynamicState& ds) const
{
    for (IStateManagerConsumer<SelfStateManager>* consumer : m_rend_consumers)
    {
        consumer->rend_on_create(handle, ss, ds);
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_notify_on_update(
    Handle handle,
    const DynamicState& ds) const
{
    for (IStateManagerConsumer<SelfStateManager>* consumer : m_rend_consumers)
    {
        consumer->rend_on_update(handle, ds);
    }
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::rend_notify_on_destroy(Handle handle) const
{
    for (IStateManagerConsumer<SelfStateManager>* consumer : m_rend_consumers)
    {
        consumer->rend_on_destroy(handle);
    }
}

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

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_reset_and_recycle_handle(
    Handle handle,
    HandleTick* handle_tick)
{
    const uint64_t handle_idx = handle.get_internal_id();

    if (handle_tick != nullptr)
        *handle_tick = HandleTick{};

    m_handle_static_states[handle_idx] = StaticState{};
    m_handle_state_info[handle_idx] = HandleStateInfo{};
    m_available_handles.push(handle_idx);
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
HandleTickCpp* BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_get_pending_handle_tick(Handle handle)
{
    const uint32_t pending_handle_idx = m_pending_handles_idx[handle.get_internal_id()];
    if (pending_handle_idx == INVALID_PENDING_IDX)
        return nullptr;

    return &m_handle_ticks[pending_handle_idx];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
const HandleTickCpp* BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_get_pending_handle_tick(
    Handle handle) const
{
    const uint32_t pending_handle_idx = m_pending_handles_idx[handle.get_internal_id()];
    if (pending_handle_idx == INVALID_PENDING_IDX)
        return nullptr;

    return &m_handle_ticks[pending_handle_idx];
}

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
void BaseStateManager2<StaticState, DynamicState, Handle, Config>::sim_validate_handle_is_alive(
    [[maybe_unused]] Handle handle) const
{
    MIZU_ASSERT(
        m_handle_state_info[handle.get_internal_id()].sim_alive,
        "Trying to access the dynamic state of a handle that is not alive on sim side");
}

} // namespace Mizu
