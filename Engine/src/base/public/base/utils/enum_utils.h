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

template <typename E>
struct enum_metadata
{
    static constexpr bool defined = false;
    static constexpr size_t count = 0;
};

#define MIZU_CREATE_ENUM_METADATA(_enum, _count) \
    template <>                                  \
    struct enum_metadata<_enum>                  \
    {                                            \
        static constexpr bool defined = true;    \
        static constexpr size_t count = _count;  \
    }

template <typename E>
concept is_enum_with_metadata = std::is_enum_v<E> && enum_metadata<E>::defined;

template <typename E>
    requires is_enum_with_metadata<E>
inline constexpr size_t enum_metadata_count_v = enum_metadata<E>::count;

} // namespace Mizu