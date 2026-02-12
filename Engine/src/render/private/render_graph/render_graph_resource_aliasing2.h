#pragma once

#include "render/render_graph/render_graph_resources.h"

namespace Mizu
{

struct AliasingResource
{
    uint32_t begin, end;
    uint64_t size, alignment, offset;
};

void render_graph_alias_resources(std::vector<AliasingResource>& resources, uint64_t& out_total_size);

} // namespace Mizu