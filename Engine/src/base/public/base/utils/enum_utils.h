#pragma once

namespace Mizu
{

#define IMPLEMENT_ENUM_FLAGS_FUNCTIONS(_enum, _type)                              \
    constexpr _enum operator|(_enum a, _enum b)                                   \
    {                                                                             \
        return static_cast<_enum>(static_cast<_type>(a) | static_cast<_type>(b)); \
    }                                                                             \
    constexpr _enum& operator|=(_enum& a, _enum b)                                \
    {                                                                             \
        a = a | b;                                                                \
        return a;                                                                 \
    }                                                                             \
    constexpr _type operator&(_enum a, _enum b)                                   \
    {                                                                             \
        return static_cast<_type>(a) & static_cast<_type>(b);                     \
    }                                                                             \
    constexpr _enum& operator&=(_enum& a, _enum b)                                \
    {                                                                             \
        a = static_cast<_enum>(a & b);                                            \
        return a;                                                                 \
    }

} // namespace Mizu