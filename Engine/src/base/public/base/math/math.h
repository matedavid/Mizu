#pragma once

#include <concepts>
#include <type_traits>

#include "base/debug/assert.h"

namespace Mizu
{
namespace math
{

template <std::integral T, std::integral U>
constexpr std::common_type_t<T, U> ceil_div(T numerator, U denominator)
{
    using R = std::common_type_t<T, U>;

    if (denominator == 0)
    {
        MIZU_ASSERT(false, "denominator cannot be zero");
        return std::numeric_limits<R>::max();
    }

    const R n = static_cast<R>(numerator);
    const R d = static_cast<R>(denominator);

    return n / d + (n % d != 0 ? 1 : 0);
}

template <std::integral T, std::integral U>
constexpr std::common_type_t<T, U> align_up(T value, U alignment)
{
    using R = std::common_type_t<T, U>;

    if (alignment == 0)
        return value;

    return ceil_div(value, alignment) * static_cast<R>(alignment);
}

template <std::integral T, std::integral U>
constexpr std::common_type_t<T, U> align_up_pow2(T value, U alignment)
{
    using R = std::common_type_t<T, U>;

    if (alignment == 0)
        return value;

    const R a = static_cast<R>(alignment);
    const R v = static_cast<R>(value);

    return (v + a - 1) & ~(a - 1);
}

template <std::integral T, std::integral U>
constexpr std::common_type_t<T, U> align_down(T value, U alignment)
{
    using R = std::common_type_t<T, U>;
    if (alignment == 0)
        return value;

    return static_cast<R>(value) / static_cast<R>(alignment) * static_cast<R>(alignment);
}

} // namespace math
} // namespace Mizu