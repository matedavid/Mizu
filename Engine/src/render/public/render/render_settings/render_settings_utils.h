#pragma once

#include <optional>

namespace Mizu
{

#define MIZU_RENDER_SETTINGS_DECLARE(_type, _name, ...) _type _name = __VA_ARGS__;

#define MIZU_RENDER_SETTINGS_DECLARE_OVERRIDE(_type, _name, ...) std::optional<_type> _name;

#define MIZU_RENDER_SETTINGS_APPLY(_type, _name, ...) \
    if (o._name.has_value())                          \
        dst._name = *o._name;

#define MIZU_RENDER_SETTINGS_CREATE(_name, _layer_overridable, _volume_overridable, _members)            \
    struct _name                                                                                         \
    {                                                                                                    \
        _members(MIZU_RENDER_SETTINGS_DECLARE)                                                           \
                                                                                                         \
            bool                                                                                         \
            operator==(const _name&) const = default;                                                    \
    };                                                                                                   \
                                                                                                         \
    struct _name##Override                                                                               \
    {                                                                                                    \
        using Settings = _name;                                                                          \
                                                                                                         \
        static constexpr bool LayerOverridable = (_layer_overridable);                                   \
        static constexpr bool VolumeOverridable = (_volume_overridable);                                 \
                                                                                                         \
        _members(MIZU_RENDER_SETTINGS_DECLARE_OVERRIDE)                                                  \
                                                                                                         \
            bool                                                                                         \
            operator==(const _name##Override&) const = default;                                          \
                                                                                                         \
        static void apply(const _name##Override& o, _name& dst) { _members(MIZU_RENDER_SETTINGS_APPLY) } \
    };

} // namespace Mizu