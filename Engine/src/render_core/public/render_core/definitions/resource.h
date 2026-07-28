#pragma once

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

} // namespace Mizu
