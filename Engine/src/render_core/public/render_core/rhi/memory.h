#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Mizu
{

using MemoryTypeId = uint32_t;

enum class DedicatedAllocation
{
    None,
    Preferred,
    Required,
};

struct MemoryRequirements
{
    uint64_t size = 0;
    uint64_t alignment = 0;

    uint32_t memory_type_bits = 0;
    DedicatedAllocation dedicated = DedicatedAllocation::None;
};

struct ImageMemoryRequirements
{
    uint64_t size = 0;
    uint64_t offset = 0;
    uint32_t row_pitch = 0;
};

struct SparseImageMemoryRequirements
{
    MemoryRequirements memory{};

    uint64_t page_size = 0;
    glm::uvec3 tile_extent{};

    uint32_t mip_tail_first_mip = 0;
    uint64_t mip_tail_size = 0;
    bool single_mip_tail = false;

    uint64_t metadata_size = 0;
};

} // namespace Mizu
