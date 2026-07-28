#pragma once

#include <magic_enum/magic_enum.hpp>
#include <magic_enum/magic_enum_containers.hpp>
#include <magic_enum/magic_enum_flags.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>

// C++, implement reflection already :)

namespace Mizu::meta
{

namespace detail
{

template <typename E>
constexpr size_t reflected_count()
{
    if constexpr (std::is_enum_v<E>)
        return magic_enum::enum_count<E>();
    else
        return 0;
}

template <typename E>
inline constexpr bool is_flags_enum_v =
    std::is_enum_v<E> && magic_enum::detail::subtype_v<E> == magic_enum::detail::enum_subtype::flags;

template <typename E>
constexpr std::string_view flags_zero_name()
{
    constexpr auto common_values = magic_enum::enum_values<E, magic_enum::detail::enum_subtype::common>();
    constexpr auto common_names = magic_enum::enum_names<E, magic_enum::detail::enum_subtype::common>();

    for (size_t i = 0; i < common_values.size(); ++i)
    {
        if (static_cast<std::underlying_type_t<E>>(common_values[i]) == std::underlying_type_t<E>{0})
            return common_names[i];
    }

    return {};
}

} // namespace detail

template <typename E>
concept ReflectedEnum = std::is_enum_v<E> && detail::reflected_count<E>() > 0;

template <typename E>
concept ReflectedFlagEnum = ReflectedEnum<E> && detail::is_flags_enum_v<E>;

template <ReflectedEnum E>
struct enum_traits
{
    using underlying_type = std::underlying_type_t<E>;

    static constexpr bool is_flags = detail::is_flags_enum_v<E>;

    static constexpr size_t count = magic_enum::enum_count<E>();
    static constexpr auto values = magic_enum::enum_values<E>();
    static constexpr auto names = magic_enum::enum_names<E>();

    static constexpr std::optional<size_t> index_of(E value) { return magic_enum::enum_index(value); }

    static constexpr bool contains(E value) { return magic_enum::enum_contains(value); }

    static constexpr std::string_view name(E value)
    {
        if constexpr (is_flags)
        {
            if (static_cast<underlying_type>(value) == underlying_type{0})
                return detail::flags_zero_name<E>();
        }

        return magic_enum::enum_name(value);
    }

    static constexpr std::optional<E> from_string(std::string_view str)
    {
        if constexpr (is_flags)
        {
            constexpr std::string_view zero_name = detail::flags_zero_name<E>();
            if (!zero_name.empty() && zero_name == str)
                return static_cast<E>(0);
        }

        return magic_enum::enum_cast<E>(str);
    }
};

template <ReflectedEnum E>
inline constexpr size_t enum_count_v = enum_traits<E>::count;

template <ReflectedEnum E>
constexpr std::string_view enum_name(E value)
{
    return enum_traits<E>::name(value);
}

template <ReflectedEnum E>
constexpr std::optional<E> enum_from_string(std::string_view str)
{
    return enum_traits<E>::from_string(str);
}

template <ReflectedFlagEnum E>
std::string enum_flags_name(E value)
{
    if (static_cast<std::underlying_type_t<E>>(value) == 0)
        return std::string(detail::flags_zero_name<E>());

    return magic_enum::enum_flags_name(value);
}

template <ReflectedFlagEnum E, typename F>
constexpr void for_each_flag(E value, F&& func)
{
    using U = std::underlying_type_t<E>;

    for (const E bit : enum_traits<E>::values)
    {
        if ((static_cast<U>(value) & static_cast<U>(bit)) != U{0})
            func(bit);
    }
}

template <ReflectedEnum E, typename T>
using enum_array = magic_enum::containers::array<E, T>;

} // namespace Mizu::meta

#define MIZU_META_ENUM_FLAGS(_enum)                                                               \
    constexpr auto magic_enum_define_range_adl(_enum)                                             \
    {                                                                                             \
        return magic_enum::customize::adl_info().flag<true>();                                    \
    }                                                                                             \
    static_assert(                                                                                \
        ::Mizu::meta::detail::is_flags_enum_v<_enum>,                                             \
        "MIZU_META_ENUM_FLAGS(" #_enum ") did not take effect. Declare it at namespace scope in " \
        "the namespace enclosing " #_enum ", after the enum definition.")

#define MIZU_META_ENUM_RANGE(_enum, _min, _max)                                                   \
    constexpr auto magic_enum_define_range_adl(_enum)                                             \
    {                                                                                             \
        return magic_enum::customize::adl_info().minmax<_min, _max>();                            \
    }                                                                                             \
    static_assert(                                                                                \
        magic_enum::customize::enum_range<_enum>::min == (_min)                                   \
            && magic_enum::customize::enum_range<_enum>::max == (_max),                           \
        "MIZU_META_ENUM_RANGE(" #_enum ") did not take effect. Declare it at namespace scope in " \
        "the namespace enclosing " #_enum ", after the enum definition.")
