#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stack>
#include <thread>

#include "state_manager/state_manager.h"

namespace Mizu
{

static constexpr uint64_t MaxTicksAhead = 5;

struct BaseStateManagerConfig2
{
    static constexpr uint64_t MaxNumHandles = 100;
    static constexpr bool Interpolate = false;
};

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
class BaseStateManager2 : public IStateManager
{
    static_assert(IsHandle<Handle>, "Invalid Handle type");
    static_assert(IsDynamicState<DynamicState>, "Invalid DynamicState type");

  public:
    BaseStateManager2();

    // Sim functions

    void sim_begin_tick(const TickUpdateState& state) override;
    void sim_end_tick() override;

    Handle sim_create(StaticState static_state, DynamicState dynamic_state);
    void sim_destroy(Handle handle);

    void sim_update(Handle handle, DynamicState dynamic_state);
    DynamicState& sim_edit(Handle handle);

    const DynamicState& sim_get_dynamic_state(Handle handle) const;

    // Rend functions

    void rend_apply_updates(const FrameUpdateState& state) override;

    const DynamicState& rend_get_dynamic_state(Handle handle) const;

    virtual void rend_on_create(
        [[maybe_unused]] Handle handle,
        [[maybe_unused]] StaticState static_state,
        [[maybe_unused]] DynamicState dynamic_state)
    {
    }
    virtual void rend_on_update([[maybe_unused]] Handle handle, [[maybe_unused]] DynamicState dynamic_state) {}
    virtual void rend_on_destroy([[maybe_unused]] Handle handle) {}

    // Other functions

    // Callable from both sim and rend
    const StaticState& get_static_state(Handle handle) const;

  private:
    std::stack<uint64_t> m_available_handles;

    std::condition_variable m_tick_consumed_cv;
    std::mutex m_tick_consumed_mutex;

    struct Tick
    {
        uint64_t tick_idx = 0;
        uint64_t sim_time_us = 0;
        uint32_t num_updated_handles = 0;
    };

    struct HandleTick
    {
        Handle handle;
        DynamicState ds;
        StateManagerEventKind event_kind;
    };

    std::atomic<uint64_t> m_last_produced_tick = 0;
    std::atomic<uint64_t> m_last_consumed_tick = 0;

    Tick* m_tick_in_production = nullptr;

    // Ring buffer of ticks
    std::array<Tick, MaxTicksAhead> m_ticks;
    std::array<HandleTick, MaxTicksAhead * Config::MaxNumHandles> m_handle_ticks;

    std::array<StaticState, Config::MaxNumHandles> m_handle_static_states;

    // Map from pending handles (that have been updated in the current tick) to their position in m_handle_ticks
    static constexpr uint32_t INVALID_PENDING_IDX = std::numeric_limits<uint32_t>::max();
    std::array<uint32_t, Config::MaxNumHandles> m_pending_handles_idx;

    struct HandleStateInfo
    {
        DynamicState sim_published_ds{};
        bool sim_alive = false;

        DynamicState consumed_ds{};
        DynamicState applied_ds{};
    };
    std::array<HandleStateInfo, Config::MaxNumHandles> m_handle_state_info;

    // Highest tick where destroyed handles have been reclaimed back into m_available_handles.
    uint64_t m_last_reclaimed_tick = 0;

    HandleTick& sim_allocate_handle_tick(Handle handle);
    void sim_reset_and_recycle_handle(Handle handle, HandleTick* handle_tick = nullptr);

    HandleTick* sim_get_pending_handle_tick(Handle handle);
    const HandleTick* sim_get_pending_handle_tick(Handle handle) const;

    void sim_validate_handle_is_alive(Handle handle) const;
};

} // namespace Mizu
