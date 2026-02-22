#pragma once

namespace Mizu
{

enum ResourceSharingMode
{
    Exclusive,
    Concurrent,
};

enum ResourceTransferMode
{
    Normal,
    Release,
    Acquire,
};

} // namespace Mizu