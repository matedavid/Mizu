#pragma once

#include <cstdint>
#include <limits>

#include "asset/asset_handle.h"
#include "base/utils/enum_utils.h"
#include "base/utils/hash.h"

namespace Mizu
{

using DrawableFlagsType = uint32_t;

enum class DrawableFlags : DrawableFlagsType
{
    None = 0,
};

IMPLEMENT_ENUM_FLAGS_FUNCTIONS(DrawableFlags, DrawableFlagsType);

struct DrawClassDesc
{
    ShaderAssetHandle vertex{};
    ShaderAssetHandle fragment{};

    bool operator==(const DrawClassDesc& other) const { return vertex == other.vertex && fragment == other.fragment; }

    size_t hash() const { return hash_compute(vertex, fragment); }
};

using DrawClassId = uint32_t;
inline constexpr DrawClassId INVALID_DRAW_CLASS_ID = std::numeric_limits<DrawClassId>::max();

} // namespace Mizu
