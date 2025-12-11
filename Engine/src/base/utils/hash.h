#pragma once

#include <functional>

namespace Mizu
{

inline void hash_combine_impl(size_t& seed, size_t value)
{
    seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename T, typename... Rest>
inline void hash_combine(size_t& seed, const T& value, const Rest&... rest)
{
    std::hash<T> hasher{};
    hash_combine_impl(seed, hasher(value));

    if constexpr (sizeof...(rest) > 0)
    {
        hash_combine(seed, rest...);
    }
}

template <typename... Args>
inline size_t hash_compute(const Args&... values)
{
    size_t seed = 0;
    hash_combine(seed, values...);

    return seed;
}

} // namespace Mizu