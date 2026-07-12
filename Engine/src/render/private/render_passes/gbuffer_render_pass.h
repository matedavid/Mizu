#pragma once

#include <cstdint>

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

struct GbufferData
{
    RenderGraphResource gbuffer0;
    RenderGraphResource gbuffer1;
    RenderGraphResource gbuffer2;
};

GbufferData create_gbuffer_data(RenderGraphBuilder& builder, uint32_t width, uint32_t height);
void add_gbuffer_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu