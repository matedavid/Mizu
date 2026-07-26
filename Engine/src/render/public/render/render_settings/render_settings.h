#pragma once

#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#include "render/render_settings/shadow_render_settings.h"

namespace Mizu
{

//
// Helpers
//

template <typename Tuple>
struct tuple_to_variant;

template <typename... Ts>
struct tuple_to_variant<std::tuple<Ts...>>
{
    using type = std::variant<Ts...>;
};

template <typename Variant, template <typename> class Pred>
struct filter_variant;

template <typename... Ts, template <typename> class Pred>
struct filter_variant<std::variant<Ts...>, Pred>
{
    using type = typename tuple_to_variant<decltype(std::tuple_cat(
        std::declval<std::conditional_t<Pred<Ts>::value, std::tuple<Ts>, std::tuple<>>>()...))>::type;
};

template <typename Variant, template <typename> class Pred>
using filter_variant_t = typename filter_variant<Variant, Pred>::type;

template <typename Variant, template <typename> class Pred>
struct transform_variant;

template <typename... Ts, template <typename> class Pred>
struct transform_variant<std::variant<Ts...>, Pred>
{
    using type = typename tuple_to_variant<std::tuple<typename Pred<Ts>::value...>>::type;
};

template <typename Variant, template <typename> class Pred>
using transform_variant_t = typename transform_variant<Variant, Pred>::type;

template <typename T, typename Variant>
inline constexpr bool is_variant_alternative_v = false;

template <typename T, typename... Ts>
inline constexpr bool is_variant_alternative_v<T, std::variant<Ts...>> = (std::is_same_v<T, Ts> || ...);

template <typename T, typename Variant>
struct variant_index;

template <typename T, typename... Ts>
struct variant_index<T, std::variant<Ts...>>
{
    static_assert(is_variant_alternative_v<T, std::variant<Ts...>>, "T is not an alternative of the variant");

  private:
    static constexpr size_t compute()
    {
        size_t index = 0;
        size_t result = sizeof...(Ts);

        ((std::is_same_v<T, Ts> ? result = index : index, ++index), ...);

        return result;
    }

  public:
    static constexpr size_t value = compute();
};

template <typename T, typename Variant>
inline constexpr size_t variant_index_v = variant_index<T, Variant>::value;

template <typename Variant>
struct variant_to_tuple;

template <typename... Ts>
struct variant_to_tuple<std::variant<Ts...>>
{
    using type = std::tuple<Ts...>;
};

//
// Definitions
//

template <typename T>
struct SettingOverridePred
{
    using value = T::Override;
};

template <typename T>
struct LayerOverridablePred : std::bool_constant<T::LayerOverridable>
{
};

template <typename T>
struct VolumeOverridablePred : std::bool_constant<T::VolumeOverridable>
{
};

using AllRenderSettingsVariant = std::variant<ShadowRenderSettings>;
using AllRenderSettingOverridesVariant = transform_variant_t<AllRenderSettingsVariant, SettingOverridePred>;

using LayerComponentOverridesVariant = filter_variant_t<AllRenderSettingOverridesVariant, LayerOverridablePred>;
using VolumeComponentOverridesVariant = filter_variant_t<AllRenderSettingOverridesVariant, VolumeOverridablePred>;

using RenderSettingsTuple = variant_to_tuple<AllRenderSettingsVariant>::type;

class ResolvedViewRenderSettings
{
  public:
    template <typename T>
    const T& resolve() const
    {
        static_assert(
            is_variant_alternative_v<T, AllRenderSettingsVariant>,
            "Cant't resolve a type that is not a RenderSetting, see AllRenderSettingsVariant for allowed types");

        return std::get<T>(m_resolved_settings);
    }

  private:
    RenderSettingsTuple m_resolved_settings{};

    friend class RenderSettingsRegistry;
};

} // namespace Mizu
