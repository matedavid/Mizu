#pragma once

#include <array>
#include <limits>
#include <queue>
#include <thread>
#include <type_traits>
#include <unordered_map>

namespace Mizu
{

#define MIZU_STATE_MANAGER_CREATE_HANDLE(_name)                 \
    struct _name                                                \
    {                                                           \
        _name() : m_id(std::numeric_limits<uint64_t>::max()) {} \
        _name(uint64_t id) : m_id(id) {}                        \
                                                                \
        uint64_t get_internal_id() const                        \
        {                                                       \
            return m_id;                                        \
        }                                                       \
                                                                \
      private:                                                  \
        uint64_t m_id;                                          \
    }

struct StateManagerConfig
{
    static constexpr uint32_t MaxNumHandles = 100;
    static constexpr uint32_t MaxStatesInFlight = 2;
};

template <typename T>
concept IsHandle = requires(std::uint64_t id) {
    T{id};
    { std::declval<const T>().get_internal_id() } -> std::same_as<std::uint64_t>;
};

template <typename StaticState, typename DynamicState, typename Handle, typename Config>
class BaseStateManager
{
    static_assert(IsHandle<Handle>, "Invalid Handle type");

    static_assert(Config::MaxNumHandles > 0, "Invalid Config::MaxNumHandles, must be > 0");
    static_assert(Config::MaxStatesInFlight > 0, "Invalid Config::MaxStatesInFlight, must be > 0");

  public:
    BaseStateManager();
    ~BaseStateManager();

    // Sim side functions

    void sim_begin_tick();
    void sim_end_tick();
    Handle sim_create_handle(StaticState static_state, DynamicState dynamic_state);
    void sim_release_handle(Handle handle);
    void sim_update(Handle handle, DynamicState dynamic_state);

    StaticState sim_get_static_state(Handle handle) const;
    DynamicState sim_get_dynamic_state(Handle handle) const;

    // Render side functions

    void rend_begin_frame();
    void rend_end_frame();

    StaticState rend_get_static_state(Handle handle) const;
    DynamicState rend_get_dynamic_state(Handle handle) const;

  private:
    std::priority_queue<uint64_t, std::vector<uint64_t>, std::greater<uint64_t>> m_available_handles;
    std::array<StaticState, Config::MaxNumHandles> m_handles_static_state;

    std::array<DynamicState, Config::MaxNumHandles * Config::MaxStatesInFlight> m_handles_dynamic_state;

    struct Fence
    {
        std::atomic<bool> is_open{true};
    };
    std::array<Fence, Config::MaxStatesInFlight> m_in_flight_fences;

    uint32_t m_sim_pos = 0;
    uint32_t m_rend_pos = 0;

    std::unordered_map<uint64_t, bool> m_requested_releases_map;

    void sim_mark_handle_for_release(uint64_t id);
    void rend_acknowledge_handle_release(uint64_t id);

    static uint32_t get_next_pos(uint32_t pos);
    static uint32_t get_prev_pos(uint32_t pos);
};

} // namespace Mizu