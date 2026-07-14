#pragma once

#include "render/render_graph/render_graph_types.h"

namespace Mizu
{

class RenderGraphBlackboard;
class RenderGraphBuilder;

struct LightingData
{
    RenderGraphResource lighting_output;
};

void add_lighting_pass(RenderGraphBuilder& builder, RenderGraphBlackboard& blackboard);

} // namespace Mizu