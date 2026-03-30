#pragma once

#include <array>
#include <cstdint>
#include <stack>
#include <thread>

#include "state_manager/state_manager.h"

namespace Mizu
{

static constexpr uint64_t MaxTicksAhead = 10;

struct BaseStateManagerConfig2
{
    static constexpr uint64_t MaxNumHandles = 100;
    static constexpr bool Interpolate = false;
};

struct TickUpdateState
{
    uint64_t sim_time_us;
};

struct FrameUpdateState
{
    uint64_t render_time_us;
};

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
class BaseStateManager2
{
    static_assert(IsHandle<Handle>, "Invalid Handle type");
    static_assert(IsDynamicState<DynamicState>, "Invalid DynamicState type");

  public:
    BaseStateManager2();

    // Sim functions

    void sim_begin_tick(const TickUpdateState& state);
    void sim_end_tick();

    Handle sim_create(StaticState static_state, DynamicState dynamic_state);
    void sim_destroy(Handle handle);

    void sim_update(Handle handle, DynamicState dynamic_state);
    DynamicState& sim_edit(Handle handle);

    // Rend functions

    void rend_apply_updates(const FrameUpdateState& state);

    virtual void rend_on_create(
        [[maybe_unused]] Handle handle,
        [[maybe_unused]] StaticState static_state,
        [[maybe_unused]] DynamicState dynamic_state)
    {
    }
    virtual void rend_on_update([[maybe_unused]] Handle handle, [[maybe_unused]] DynamicState dynamic_state) {}
    virtual void rend_on_destroy([[maybe_unused]] Handle handle) {}

  private:
    std::stack<uint64_t> m_available_handles;

    struct Tick
    {
        uint64_t tick_idx = 0;
        uint64_t sim_time_us = 0;
        uint32_t num_updated_handles = 0;
    };

    struct HandleTick
    {
        Handle handle;
        StateManagerEventKind event_kind;
        StaticState ss;
        DynamicState ds;
    };

    std::atomic<uint64_t> m_last_produced_tick = 0;
    std::atomic<uint64_t> m_last_consumed_tick = 0;

    Tick* m_tick_in_production = nullptr;

    // Ring buffer of ticks
    std::array<Tick, MaxTicksAhead> m_ticks;
    std::array<HandleTick, MaxTicksAhead * Config::MaxNumHandles> m_handle_ticks;

    // Map from pending handles (that have been updated in the current tick) to their position in m_handle_ticks
    static constexpr uint32_t INVALID_PENDING_IDX = std::numeric_limits<uint32_t>::max();
    std::array<uint32_t, Config::MaxNumHandles> m_pending_handles_idx;

    std::array<HandleTick, Config::MaxNumHandles> m_rend_last_consumed_handle_tick;

    // Highest tick where destroyed handles have been reclaimed back into m_available_handles.
    uint64_t m_last_reclaimed_tick = 0;

    HandleTick& sim_allocate_handle_tick(Handle handle);
};

} // namespace Mizu
