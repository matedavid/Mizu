#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Mizu
{

constexpr uint64_t INVALID_HANDLE_ID = std::numeric_limits<uint64_t>::max();
constexpr uint64_t INVALID_HANDLE_GENERATION = std::numeric_limits<uint64_t>::max();

#define MIZU_STATE_MANAGER_CREATE_HANDLE(_name)                                                                        \
    struct _name                                                                                                       \
    {                                                                                                                  \
        _name() : m_id(Mizu::INVALID_HANDLE_ID), m_generation(Mizu::INVALID_HANDLE_GENERATION) {}                      \
        _name(uint64_t id, uint64_t generation) : m_id(id), m_generation(generation) {}                                \
                                                                                                                       \
        _name(const _name& other)                                                                                      \
        {                                                                                                              \
            m_id = other.m_id;                                                                                         \
            m_generation = other.m_generation;                                                                         \
        }                                                                                                              \
                                                                                                                       \
        _name& operator=(const _name& other)                                                                           \
        {                                                                                                              \
            m_id = other.m_id;                                                                                         \
            m_generation = other.m_generation;                                                                         \
            return *this;                                                                                              \
        }                                                                                                              \
                                                                                                                       \
        bool is_valid() const                                                                                          \
        {                                                                                                              \
            return m_id != Mizu::INVALID_HANDLE_ID && m_generation != Mizu::INVALID_HANDLE_GENERATION;                 \
        }                                                                                                              \
                                                                                                                       \
        bool operator==(const _name& other) const { return m_id == other.m_id && m_generation == other.m_generation; } \
                                                                                                                       \
        uint64_t get_internal_id() const { return m_id; }                                                              \
        uint64_t get_generation() const { return m_generation; }                                                       \
                                                                                                                       \
      private:                                                                                                         \
        uint64_t m_id;                                                                                                 \
        uint64_t m_generation;                                                                                         \
    }

// clang-format off
template <typename T>
concept IsHandle = requires(uint64_t id, uint64_t generation) {
    T{id, generation};
    { std::declval<const T>().get_internal_id() } -> std::same_as<uint64_t>;
    { std::declval<const T>().get_generation() } -> std::same_as<uint64_t>;
    { std::declval<const T>().is_valid() } -> std::same_as<bool>;
};

template <typename T>
concept DynamicStateHasInterpolate = requires(const T& t, const T& ds, double alpha) {
    { t.interpolate(ds, alpha) } -> std::convertible_to<T>;
};

template <typename T, bool Interpolate>
concept IsDynamicState = requires(const T& t, const T& ds) {
    { t.has_changed(ds) } -> std::convertible_to<bool>;
} && (!Interpolate || DynamicStateHasInterpolate<T>);

template <typename T>
concept IsConfig = requires {
    { T::MaxNumHandles } -> std::same_as<const uint64_t&>;
    { T::Interpolate } -> std::same_as<const bool&>;
    { T::Identifier } -> std::same_as<const std::string_view&>;

    requires T::MaxNumHandles >= 1;
    requires !T::Identifier.empty();
};
// clang-format on

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
