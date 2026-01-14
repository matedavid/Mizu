#pragma once

#include <cstdint>
#include <numeric>

namespace Mizu
{

#define MIZU_STATE_MANAGER_CREATE_HANDLE(_name)                                           \
    struct _name##Functions;                                                              \
    struct _name                                                                          \
    {                                                                                     \
        using FunctionsT = _name##Functions;                                              \
                                                                                          \
        _name() : m_id(InvalidHandleId) {}                                                \
        _name(uint64_t id) : m_id(id) {}                                                  \
                                                                                          \
        bool is_valid() const                                                             \
        {                                                                                 \
            return m_id != InvalidHandleId;                                               \
        }                                                                                 \
                                                                                          \
        bool operator==(const _name& other) const                                         \
        {                                                                                 \
            return m_id == other.m_id;                                                    \
        }                                                                                 \
                                                                                          \
        uint64_t get_internal_id() const                                                  \
        {                                                                                 \
            return m_id;                                                                  \
        }                                                                                 \
                                                                                          \
        FunctionsT* operator->()                                                          \
        {                                                                                 \
            return reinterpret_cast<FunctionsT*>(this);                                   \
        }                                                                                 \
                                                                                          \
        const FunctionsT* operator->() const                                              \
        {                                                                                 \
            return reinterpret_cast<FunctionsT*>(const_cast<_name*>(this));               \
        }                                                                                 \
                                                                                          \
      private:                                                                            \
        uint64_t m_id;                                                                    \
                                                                                          \
        static constexpr uint64_t InvalidHandleId = std::numeric_limits<uint64_t>::max(); \
    }

template <typename T>
concept IsHandle = requires(uint64_t id) {
    T{id};
    { std::declval<const T>().get_internal_id() } -> std::same_as<uint64_t>;
};

} // namespace Mizu