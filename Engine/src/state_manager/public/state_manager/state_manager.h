#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Mizu
{

constexpr uint64_t INVALID_HANDLE_ID = std::numeric_limits<uint64_t>::max();

#define MIZU_STATE_MANAGER_CREATE_HANDLE(_name)                             \
    struct _name##Functions;                                                \
    struct _name                                                            \
    {                                                                       \
        using FunctionsT = _name##Functions;                                \
                                                                            \
        _name() : m_id(Mizu::INVALID_HANDLE_ID) {}                          \
        _name(uint64_t id) : m_id(id) {}                                    \
                                                                            \
        bool is_valid() const                                               \
        {                                                                   \
            return m_id != Mizu::INVALID_HANDLE_ID;                         \
        }                                                                   \
                                                                            \
        bool operator==(const _name& other) const                           \
        {                                                                   \
            return m_id == other.m_id;                                      \
        }                                                                   \
                                                                            \
        uint64_t get_internal_id() const                                    \
        {                                                                   \
            return m_id;                                                    \
        }                                                                   \
                                                                            \
        FunctionsT* operator->()                                            \
        {                                                                   \
            return reinterpret_cast<FunctionsT*>(this);                     \
        }                                                                   \
                                                                            \
        const FunctionsT* operator->() const                                \
        {                                                                   \
            return reinterpret_cast<FunctionsT*>(const_cast<_name*>(this)); \
        }                                                                   \
                                                                            \
      private:                                                              \
        uint64_t m_id;                                                      \
    }

template <typename T>
concept IsHandle = requires(uint64_t id) {
    T{id};
    { std::declval<const T>().get_internal_id() } -> std::same_as<uint64_t>;
    { std::declval<const T>().is_valid() } -> std::same_as<bool>;
};

template <typename T>
concept IsDynamicState = requires(const T& ds, double alpha) {
    { std::declval<const T>().has_changed(ds) } -> std::convertible_to<bool>;
    { std::declval<const T>().interpolate(ds, alpha) } -> std::convertible_to<T>;
};

enum class StateManagerEventKind : uint8_t
{
    Create,
    Update,
    Destroy,
};

struct TickUpdateState
{
    uint64_t sim_time_us;
};

struct FrameUpdateState
{
    uint64_t render_time_us;
};

class IStateManager
{
  public:
    virtual ~IStateManager() = default;

    virtual void sim_begin_tick(const TickUpdateState& state) = 0;
    virtual void sim_end_tick() = 0;

    virtual void rend_apply_updates(const FrameUpdateState& state) = 0;

    virtual std::string_view get_identifier() const = 0;
};

} // namespace Mizu
