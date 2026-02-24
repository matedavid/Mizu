#include "render_core/definitions/resource.h"

namespace Mizu
{

#if MIZU_DEBUG

std::string_view resource_sharing_mode_to_string(ResourceSharingMode mode)
{
    switch (mode)
    {
    case ResourceSharingMode::Exclusive:
        return "Exclusive";
    case ResourceSharingMode::Concurrent:
        return "Concurrent";
    }
}

std::string_view resource_transition_mode_to_string(ResourceTransitionMode mode)
{
    switch (mode)
    {
    case ResourceTransitionMode::Normal:
        return "Normal";
    case ResourceTransitionMode::Release:
        return "Release";
    case ResourceTransitionMode::Acquire:
        return "Acquire";
    }
}

#endif

} // namespace Mizu