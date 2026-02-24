#pragma once

#include <string_view>

#include "mizu_render_core_module.h"

namespace Mizu
{

enum ResourceSharingMode
{
    Exclusive,
    Concurrent,
};

enum ResourceTransitionMode
{
    Normal,
    Release,
    Acquire,
};

#if MIZU_DEBUG
MIZU_RENDER_CORE_API std::string_view resource_sharing_mode_to_string(ResourceSharingMode mode);
MIZU_RENDER_CORE_API std::string_view resource_transition_mode_to_string(ResourceTransitionMode mode);
#endif

} // namespace Mizu