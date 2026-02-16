#pragma once

#include <bitset>

#include "base/debug/assert.h"
#include "base/utils/enum_utils.h"

namespace Mizu
{

template <typename T>
class typed_bitset : public std::bitset<enum_metadata_count_v<T>>
{
    static_assert(is_enum_with_metadata<T>, "Enum type is not valid");

    using BitsetBase = std::bitset<enum_metadata_count_v<T>>;

  public:
    constexpr bool operator[](T pos) const { return test(get_positoin(pos)); }

    constexpr bool test(T pos) { return BitsetBase::test(get_position(pos)); }

    constexpr void set(T pos, bool value = true) { BitsetBase::set(get_position(pos), value); }

    constexpr void reset(T pos) { BitsetBase::reset(get_position(pos)); }

  private:
    constexpr size_t get_position(T pos) const
    {
        const size_t num_pos = static_cast<size_t>(pos);
        MIZU_ASSERT(num_pos < BitsetBase::size(), "Invalid enum");
        return num_pos;
    }
};

} // namespace Mizu