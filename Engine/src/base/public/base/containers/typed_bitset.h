#pragma once

#include <bitset>
#include <optional>

#include "base/debug/assert.h"
#include "base/reflection/enum_traits.h"

namespace Mizu
{

template <meta::ReflectedEnum T>
class typed_bitset : public std::bitset<meta::enum_count_v<T>>
{
    using BitsetBase = std::bitset<meta::enum_count_v<T>>;

  public:
    constexpr bool operator[](T pos) const { return test(get_position(pos)); }

    constexpr bool test(T pos) { return BitsetBase::test(get_position(pos)); }

    constexpr void set(T pos, bool value = true) { BitsetBase::set(get_position(pos), value); }

    constexpr void reset(T pos) { BitsetBase::reset(get_position(pos)); }

    using BitsetBase::to_ulong;

  private:
    constexpr size_t get_position(T pos) const
    {
        const std::optional<size_t> index = meta::enum_traits<T>::index_of(pos);
        MIZU_ASSERT(index.has_value(), "Invalid enum");
        return index.value_or(0);
    }
};

} // namespace Mizu
