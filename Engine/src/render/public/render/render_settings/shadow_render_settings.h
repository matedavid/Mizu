#pragma once

#include <array>
#include <cstdint>

#include "render/render_settings/render_settings_utils.h"

namespace Mizu
{

inline constexpr uint32_t SHADOW_MAX_NUM_CASCADES = 10;
inline constexpr uint32_t SHADOW_MIN_RESOLUTION = 64;

using ShadowCascadeSplits = std::array<float, SHADOW_MAX_NUM_CASCADES>;

#define SHADOW_RENDER_SETTINGS_MEMBERS(_MEMBER) \
    _MEMBER(uint32_t, resolution, 2048)         \
    _MEMBER(uint32_t, num_cascades, 4)          \
    _MEMBER(                                    \
        ShadowCascadeSplits,                    \
        cascade_split_factors,                  \
        ShadowCascadeSplits{0.05f, 0.15f, 0.50f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f})

MIZU_RENDER_SETTINGS_CREATE(ShadowRenderSettings, /* layer */ true, /* volume */ true, SHADOW_RENDER_SETTINGS_MEMBERS)

#undef SHADOW_RENDER_SETTINGS_MEMBERS

} // namespace Mizu
