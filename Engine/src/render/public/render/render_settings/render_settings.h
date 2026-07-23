#pragma once

#include <cstdint>
#include <optional>
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

template <typename T, typename Variant>
inline constexpr bool is_variant_alternative_v = false;

template <typename T, typename... Ts>
inline constexpr bool is_variant_alternative_v<T, std::variant<Ts...>> = (std::is_same_v<T, Ts> || ...);

//
// Definitions
//

template <typename T>
struct LayerOverridable : std::bool_constant<T::LayerOverridable>
{
};

template <typename T>
struct VolumeOverridable : std::bool_constant<T::VolumeOverridable>
{
};

using AllRenderSettingsOverridesVariant = std::variant<ShadowRenderSettingsOverride>;

using LayerComponentOverridesVariant = filter_variant_t<AllRenderSettingsOverridesVariant, LayerOverridable>;
using VolumeComponentOverridesVariant = filter_variant_t<AllRenderSettingsOverridesVariant, VolumeOverridable>;

} // namespace Mizu
