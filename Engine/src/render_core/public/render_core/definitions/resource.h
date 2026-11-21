#pragma once

namespace Mizu
{

enum class ResourceSharingMode
{
    Exclusive,
    Concurrent,
};

enum class ResourceTransitionMode
{
    Normal,
    Release,
    Acquire,
};

} // namespace Mizu
