#pragma once

namespace Mizu
{

struct MemoryRequirements
{
    size_t size = 0;
    size_t alignment = 0;

    uint32_t memory_type_bits = 0; // Vulkan only
};

struct ImageMemoryRequirements
{
    size_t size;
    size_t offset;
    size_t row_pitch;
};

} // namespace Mizu