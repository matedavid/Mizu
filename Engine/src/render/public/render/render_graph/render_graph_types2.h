#pragma once

#include <cstdint>
#include <limits>

namespace Mizu
{

constexpr uint64_t INVALID_RENDER_GRAPH_RESOURCE = std::numeric_limits<uint64_t>::max();

struct RenderGraphResource
{
    uint64_t id = INVALID_RENDER_GRAPH_RESOURCE;
};

} // namespace Mizu
