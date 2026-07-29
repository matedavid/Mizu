#pragma once

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <typeinfo>

#include "base/reflection/enum_traits.h"

namespace Mizu
{

struct SettingTypeReflection
{
    bool (*parse)(std::string_view str, uint8_t* dst);
    // Only reports allowed values for enums, other types is empty..
    std::span<const std::string_view> (*allowed_values)();
};

template <typename T>
struct setting_value_traits
{
    static constexpr bool supported = meta::ReflectedEnum<T> || std::is_same_v<T, bool> || std::is_arithmetic_v<T>;

    static bool parse(std::string_view str, T& out)
    {
        if constexpr (meta::ReflectedEnum<T>)
        {
            const std::optional<T> value = meta::enum_from_string<T>(str);
            if (!value.has_value())
                return false;

            out = *value;
            return true;
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (str == "true" || str == "1")
            {
                out = true;
                return true;
            }

            if (str == "false" || str == "0")
            {
                out = false;
                return true;
            }

            return false;
        }
        else if constexpr (std::is_arithmetic_v<T>)
        {
            const auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), out);
            return ec == std::errc{} && ptr == str.data() + str.size();
        }
        else
        {
            return false;
        }
    }

    static std::span<const std::string_view> allowed_values()
    {
        if constexpr (meta::ReflectedEnum<T>)
            return meta::enum_traits<T>::names;
        else
            return {};
    }
};

template <typename T>
bool parse_setting_member(std::string_view str, uint8_t* dst)
{
    T value{};
    if (!setting_value_traits<T>::parse(str, value))
        return false;

    std::memcpy(dst, &value, sizeof(T));
    return true;
}

template <typename T>
std::span<const std::string_view> setting_member_allowed_values()
{
    return setting_value_traits<T>::allowed_values();
}

template <typename T>
const SettingTypeReflection& setting_type_reflection()
{
    static_assert(setting_value_traits<T>::supported, "Setting member type is not supported.");

    static constexpr SettingTypeReflection reflection{
        .parse = &parse_setting_member<T>,
        .allowed_values = &setting_member_allowed_values<T>,
    };

    return reflection;
}

struct SettingMember
{
    std::string_view name;
    const std::type_info& type;
    size_t size;
    size_t offset;
    const SettingTypeReflection& type_reflection;
};

#define MIZU_SETTING_STRUCT_MEMBER(_type, _name, _default_value) _type _name = _default_value;

#define MIZU_SETTING_MEMBERS_VECTOR(_type, _name, _default_value)  \
    Mizu::SettingMember{                                           \
        .name = #_name,                                            \
        .type = typeid(_type),                                     \
        .size = sizeof(_type),                                     \
        .offset = offsetof(SettingStructT, _name),                 \
        .type_reflection = Mizu::setting_type_reflection<_type>(), \
    },

#define MIZU_CREATE_SETTING(_struct_name, _members)                                    \
    struct _struct_name                                                                \
    {                                                                                  \
        using SettingStructT = _struct_name;                                           \
                                                                                       \
        static constexpr std::string_view get_name() { return #_struct_name; }         \
                                                                                       \
        static std::span<const Mizu::SettingMember> get_members()                      \
        {                                                                              \
            static const std::array members = {_members(MIZU_SETTING_MEMBERS_VECTOR)}; \
            return members;                                                            \
        }                                                                              \
                                                                                       \
        _members(MIZU_SETTING_STRUCT_MEMBER)                                           \
    }

} // namespace Mizu
