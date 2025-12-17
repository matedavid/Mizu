#pragma once

namespace Mizu
{

struct MemoryRequirements
{
    size_t size;
    size_t alignment;
};

struct ImageMemoryRequirements
{
    size_t size;
    size_t offset;
    size_t row_pitch;
};

} // namespace Mizu